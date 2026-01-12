#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <math.h>
#include <stdio.h>
#define TAG "FlipDoctor"

/* ================= CONFIG ================= */
#define SCREEN_W 128
#define SCREEN_H 64

#define STICK_LEN 10
#define PEG_RADIUS 1.5f 
#define PEG_SPACING 13
#define PEG_MARGIN 4 

#define ANG_SPEED 0.06f
#define SNAP_DIST 5.0f 
#define TICK_MS 30

#define PEG_ROWS 5

// This must match the binary file structure exactly
#define LEVEL_MAGIC 0xDEADC0DE
#define LEVEL_FILE_PATH EXT_PATH("apps_data/flip_doctor/level.bin")

/* ================= TYPES ================= */
typedef struct { int x, y; } Peg;

typedef struct {
    uint32_t magic;      
    int32_t goal_idx;        
    int32_t enemy_peg_idx;   
    int32_t wall_x;          
    int32_t wall_y;
    int32_t wall_w;
    int32_t wall_h;
} LevelData;

typedef struct {
    int anchor;
    float angle;
    int spin_dir;
} Stick;

typedef struct {
    int peg_index;
    float angle;
} Enemy;

typedef struct {
    int x, y, w, h;
} Wall;

typedef struct {
    bool running;
    bool won;
    bool lost;
    uint32_t frame_count;
    uint32_t start_tick;
    float final_time;
    Stick stick;
    int goal_idx;
    Enemy enemy;
    Wall wall;
} GameState;

/*
static const NotificationSequence sequence_level_completed = {    
    &message_vibro_on,
    &message_delay_10,
    &message_delay_10,
    &message_vibro_off,

    // CYAN (green + blue)
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_delay_100,

    // MAGENTA (red + blue)
    &message_red_255,
    &message_green_0,
    &message_blue_255,
    &message_delay_100,

    &message_vibro_on,
    &message_delay_10,
    &message_delay_10,
    &message_vibro_off,

    // Turn off
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};
*/

static const NotificationSequence sequence_snap = {    
    &message_vibro_on,
    &message_delay_10,
    &message_delay_10,
    &message_vibro_off,
    NULL,
};

/* ================= UTILS ================= */
static int clamp(int val, int min, int max) {
    if(val < min) return min;
    if(val > max) return max;
    return val;
}

static float dist_sq(float x1, float y1, float x2, float y2) {
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

/* ================= GLOBALS ================= */
static Peg* pegs = NULL;
static int peg_count = 0;
static GameState game;
static ViewPort* viewport;
static FuriMutex* mutex;
static NotificationApp* notifications;

/* ================= STORAGE LOGIC ================= */
static bool load_level_from_file() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    LevelData loaded_level;
    bool success = false;

    if(storage_file_open(file, LEVEL_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read_size = storage_file_read(file, &loaded_level, sizeof(LevelData));
        if(read_size == sizeof(LevelData) && loaded_level.magic == LEVEL_MAGIC) {
            game.goal_idx = (int)loaded_level.goal_idx;
            game.enemy.peg_index = (int)loaded_level.enemy_peg_idx;
            game.wall = (Wall){
                (int)loaded_level.wall_x, 
                (int)loaded_level.wall_y, 
                (int)loaded_level.wall_w, 
                (int)loaded_level.wall_h
            };
            FURI_LOG_I(TAG, "Level loaded successfully from SD");
            success = true;
        } else {
            FURI_LOG_E(TAG, "Level file magic mismatch or size error");
        }
    } else {
        FURI_LOG_W(TAG, "No level file found at %s, using defaults", LEVEL_FILE_PATH);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

/* ================= GAME LOGIC ================= */
static void build_pegs() {
    int cols = (SCREEN_W - (PEG_MARGIN * 2)) / PEG_SPACING + 1;
    int rows = PEG_ROWS;
    peg_count = cols * rows;
    pegs = malloc(sizeof(Peg) * peg_count);
    int i = 0;
    for(int r = 0; r < rows; r++) {
        for(int c = 0; c < cols; c++) {
            pegs[i++] = (Peg){PEG_MARGIN + c * PEG_SPACING, PEG_MARGIN + r * PEG_SPACING};
        }
    }
}

static void reset_game() {
    game.won = false;
    game.lost = false;
    game.frame_count = 0;
    game.start_tick = furi_get_tick();
    game.stick.anchor = 0;
    game.stick.angle = 0.0f;
    game.stick.spin_dir = 1;
    game.goal_idx = peg_count - 1; 
    game.enemy.peg_index = peg_count / 2;
    game.enemy.angle = 0.0f;
    //game.wall = (Wall){80, 10, 4, 30};
    // Try to load from SD card, otherwise use hardcoded defaults
    if(!load_level_from_file()) {
        game.goal_idx = peg_count - 1; 
        game.enemy.peg_index = peg_count / 2;
        game.wall = (Wall){80, 10, 4, 30};
    }
}

static bool try_snap() {
    Stick* s = &game.stick;
    Peg a = pegs[s->anchor];
    float fx = (float)a.x + cosf(s->angle) * STICK_LEN;
    float fy = (float)a.y + sinf(s->angle) * STICK_LEN;

    int best = -1;
    float best_d2 = SNAP_DIST * SNAP_DIST;

    for(int i = 0; i < peg_count; i++) {
        if(i == s->anchor) continue;
        float d2 = dist_sq(fx, fy, (float)pegs[i].x, (float)pegs[i].y);
        if(d2 < best_d2) {
            best_d2 = d2;
            best = i;
        }
    }

    if(best >= 0) {
        float old_ax = (float)a.x;
        float old_ay = (float)a.y;
        s->anchor = best;
        s->angle = atan2f(old_ay - (float)pegs[best].y, old_ax - (float)pegs[best].x);
        
        // Trigger vibration on snap
        notification_message(notifications, &sequence_snap);
        
        if(s->anchor == game.goal_idx) {
            game.won = true;
            game.final_time = (float)(furi_get_tick() - game.start_tick) / 1000.0f;
        }
        return true;
    }
    return false;
}

static void update_physics() {
    if(game.won || game.lost) return;
    game.frame_count++;
    
    game.stick.angle += ANG_SPEED * game.stick.spin_dir;
    game.enemy.angle -= ANG_SPEED * 0.7f;

    Peg a = pegs[game.stick.anchor];
    float px = (float)a.x + cosf(game.stick.angle) * STICK_LEN;
    float py = (float)a.y + sinf(game.stick.angle) * STICK_LEN;

    // Enemy Collision (Tip to Tip)
    Peg ep = pegs[game.enemy.peg_index];
    float ex = (float)ep.x + cosf(game.enemy.angle) * STICK_LEN;
    float ey = (float)ep.y + sinf(game.enemy.angle) * STICK_LEN;
    if(dist_sq(px, py, ex, ey) < 12.0f) game.lost = true;

    // Wall Bounce Logic
    if(px > (float)game.wall.x && px < (float)(game.wall.x + game.wall.w) &&
       py > (float)game.wall.y && py < (float)(game.wall.y + game.wall.h)) {
        game.stick.spin_dir *= -1;
        // Adjust angle slightly to clear the wall collision box
        game.stick.angle += (ANG_SPEED * 2.1f) * game.stick.spin_dir;
    }
}

/* ================= DRAW ================= */
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    furi_mutex_acquire(mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(game.won) {
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "TIME: %.2fs", (double)game.final_time);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "LEVEL COMPLETE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, time_str);
        //notification_message(notifications, &sequence_level_completed);
    } else if(game.lost) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "FAIL - TRY AGAIN");
        notification_message(notifications, &sequence_error);
    } else {
        // Draw Dot Grid
        for(int i = 0; i < peg_count; i++) {
            canvas_draw_circle(canvas, pegs[i].x, pegs[i].y, 1);
        }

        // Finish Star: 4 Outward Triangles + 1 Circle
        int gx = pegs[game.goal_idx].x; 
        int gy = pegs[game.goal_idx].y;
        canvas_draw_circle(canvas, gx, gy, 4);
        // Top triangle (out)
        canvas_draw_line(canvas, gx-2, gy-4, gx+2, gy-4); canvas_draw_line(canvas, gx-2, gy-4, gx, gy-7); canvas_draw_line(canvas, gx+2, gy-4, gx, gy-7);
        // Bottom triangle (out)
        canvas_draw_line(canvas, gx-2, gy+4, gx+2, gy+4); canvas_draw_line(canvas, gx-2, gy+4, gx, gy+7); canvas_draw_line(canvas, gx+2, gy+4, gx, gy+7);
        // Left triangle (out)
        canvas_draw_line(canvas, gx-4, gy-2, gx-4, gy+2); canvas_draw_line(canvas, gx-4, gy-2, gx-7, gy); canvas_draw_line(canvas, gx-4, gy+2, gx-7, gy);
        // Right triangle (out)
        canvas_draw_line(canvas, gx+4, gy-2, gx+4, gy+2); canvas_draw_line(canvas, gx+4, gy-2, gx+7, gy); canvas_draw_line(canvas, gx+4, gy+2, gx+7, gy);

        // Obstacle Wall
        canvas_draw_box(canvas, game.wall.x, game.wall.y, game.wall.w, game.wall.h);

        // Enemy (Clipped for screen boundaries)
        Peg ep = pegs[game.enemy.peg_index];
        int ex2 = clamp((int)(ep.x + cosf(game.enemy.angle) * STICK_LEN), 0, 127);
        int ey2 = clamp((int)(ep.y + sinf(game.enemy.angle) * STICK_LEN), 0, 63);
        canvas_draw_line(canvas, ep.x, ep.y, ex2, ey2);
        canvas_draw_disc(canvas, ep.x, ep.y, 2);

        // Player (Thickened line + Single anchor circle)
        Peg a = pegs[game.stick.anchor];
        int px2 = clamp((int)(a.x + cosf(game.stick.angle) * STICK_LEN), 0, 127);
        int py2 = clamp((int)(a.y + sinf(game.stick.angle) * STICK_LEN), 0, 63);
        canvas_draw_line(canvas, a.x, a.y, px2, py2);
        canvas_draw_line(canvas, a.x+1, a.y, px2+1, py2);
        canvas_draw_circle(canvas, a.x, a.y, 4);
    }
    furi_mutex_release(mutex);
}

/* ================= INPUT ================= */
static void input_callback(InputEvent* event, void* ctx) {
    UNUSED(ctx);
    if(event->type != InputTypePress) return;
    furi_mutex_acquire(mutex, FuriWaitForever);
    if(game.won || game.lost) {
        reset_game();
    } else {
        if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            game.stick.spin_dir *= -1;
        } else if(event->key == InputKeyOk) {
            try_snap();
        }
    }
    if(event->key == InputKeyBack) game.running = false;
    furi_mutex_release(mutex);
}

/* ================= APP ENTRY ================= */
int32_t flip_doctor_app(void* p) {
    UNUSED(p);
    mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    notifications = furi_record_open(RECORD_NOTIFICATION);
    build_pegs();
    reset_game();
    game.running = true;

    viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw_callback, NULL);
    view_port_input_callback_set(viewport, input_callback, NULL);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(game.running) {
        furi_mutex_acquire(mutex, FuriWaitForever);
        update_physics();
        furi_mutex_release(mutex);
        view_port_update(viewport);
        furi_delay_ms(TICK_MS);
    }

    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    if(pegs) free(pegs);
    furi_mutex_free(mutex);
    return 0;
}