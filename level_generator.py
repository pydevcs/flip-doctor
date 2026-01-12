import struct

# Format: Magic(I), GoalIdx(i), EnemyIdx(i), WallX(i), WallY(i), WallW(i), WallH(i)
# 'I' is unsigned int, 'i' is signed int. < means Little Endian (ARM)
magic = 0xDEADC0DE
goal_idx = 42
enemy_idx = 15
wall = (50, 20, 10, 40) # x, y, w, h

with open("level.bin", "wb") as f:
    f.write(struct.pack("<Iiiiiii", magic, goal_idx, enemy_idx, *wall))