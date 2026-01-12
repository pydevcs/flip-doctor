import struct
import random

# CONFIG - Must match your C code
LEVEL_MAGIC = 0xDEADC0DE
SCREEN_W = 128
SCREEN_H = 64
PEG_SPACING = 13
PEG_MARGIN = 4
PEG_ROWS = 5
COLS = (SCREEN_W - (PEG_MARGIN * 2)) // PEG_SPACING + 1
TOTAL_PEGS = COLS * PEG_ROWS

def generate_random_level(filename="level.bin"):
    # 1. Randomize Goal (avoiding the starting peg at index 0)
    goal_idx = random.randint(1, TOTAL_PEGS - 1)
    
    # 2. Randomize Enemy (must be different from start and goal)
    enemy_idx = random.choice([i for i in range(1, TOTAL_PEGS) if i != goal_idx])
    
    # 3. Randomize Wall
    # We'll pick a random spot in the middle of the screen
    wall_w = random.randint(4, 12)
    wall_h = random.randint(15, 35)
    wall_x = random.randint(30, 90)
    wall_y = random.randint(5, 20)

    # 4. Pack into Binary
    # Format: Magic (I), Goal(i), Enemy(i), WallX(i), WallY(i), WallW(i), WallH(i)
    # '<' ensures Little Endian for the Flipper's ARM processor
    binary_data = struct.pack(
        "<Iiiiiii", 
        LEVEL_MAGIC, 
        goal_idx, 
        enemy_idx, 
        wall_x, 
        wall_y, 
        wall_w, 
        wall_h
    )

    with open(filename, "wb") as f:
        f.write(binary_data)
    
    print(f"Level generated: {filename}")
    print(f"Goal Peg: {goal_idx} | Enemy Peg: {enemy_idx}")
    print(f"Wall: x={wall_x}, y={wall_y}, w={wall_w}, h={wall_h}")

if __name__ == "__main__":
    generate_random_level()