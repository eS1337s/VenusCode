from matplotlib import pyplot as plt
from matplotlib.pyplot import figure
from random import randrange
import numpy as np

i = 0
plt.grid(linewidth=0.5, color='gray', linestyle='--')
robotAPosition = [1, -4]
robotBPosition = [-2, 3]

# Lists to store positions of 'x' markers
rockCount = []
rockAPosition = []
rockBPosition = []

while i < 100:
    robotAPosition[randrange(0, 2)] += 1
    robotBPosition[randrange(0, 2)] += 1

    plt.clf()  # Clear the current figure to redraw
    plt.grid(linewidth=0.5, color='gray', linestyle='--')

    # Calculate dynamic limits to keep (0, 0) centered
    max_x = max(abs(robotAPosition[0]), abs(robotBPosition[0]))
    max_y = max(abs(robotAPosition[1]), abs(robotBPosition[1]))
    limit = max(max_x, max_y) + 10  # Add some padding
    plt.xlim(-limit, limit)
    plt.ylim(-limit, limit)

    # Plot the current positions of the robots
    plt.plot(robotAPosition[0], robotAPosition[1], 'bo', label='Robot A')
    plt.plot(robotBPosition[0], robotBPosition[1], 'ro', label='Robot B')

    # Add 'x' markers at specific intervals and store their positions
    if i == 5 or i == 10 or i == 15:
        rockCount.append(len(rockCount) + 1)  # Increment rock count
        rockAPosition.append(tuple(robotAPosition))
        rockBPosition.append(tuple(robotBPosition))

    # Plot all stored 'x' markers
    for idx, pos in enumerate(rockAPosition):
        plt.plot(pos[0], pos[1], 'bx', label=f'Rock {rockCount[idx]}')
    for idx, pos in enumerate(rockBPosition):
        plt.plot(pos[0], pos[1], 'rx', label=f'Rock {rockCount[idx]}')

    plt.title('Robot Movement')
    plt.xlabel('X-axis')
    plt.ylabel('Y-axis')

    plt.legend()
    plt.pause(0.5)  # Pause for 0.5 seconds to simulate real-time movement
    i += 1

plt.show()


