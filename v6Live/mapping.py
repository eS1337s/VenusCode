from matplotlib import pyplot as plt

def plot(aData, bData):  # [x,y,rock,colour, size ,cliff,mountain]
                          # [1,2,3   ,4,    ,5,   ,6]

    plt.rcParams['figure.figsize'] = (12, 6)
    plt.rcParams['figure.max_open_warning'] = 50
    plt.rcParams['figure.facecolor'] = 'silver'

    #Initialize 
    if not hasattr(plot, "initialized"):
        plot.robotAPosition = [1, 0]
        plot.robotBPosition = [-1, 0]
        plot.rockAPosition = []
        plot.rockBPosition = []
        plot.cliffAPosition = []
        plot.cliffBPosition = []
        plot.mountainAPosition = []
        plot.mountainBPosition = []
        plot.rockCount = 0
        plt.ion() 

        plot.fig, plot.ax = plt.subplots()
        plot.fig.canvas.manager.set_window_title("To Venus and Beyond!")
        plot.initialized = True
        plt.show(block=False)

    #updata Positions
    plot.robotAPosition[0] += aData["x"]
    plot.robotAPosition[1] += aData["y"]
    plot.robotBPosition[0] -= bData["x"]
    plot.robotBPosition[1] -= bData["y"]

    #clear
    plot.ax.clear()
    plot.ax.set_facecolor('peru')
    plot.ax.grid(linewidth=0.5, color='gray', linestyle='--')

    zoom_range = 15
    plot.ax.set_xlim(-zoom_range, zoom_range)
    plot.ax.set_ylim(-zoom_range, zoom_range)

    #plot
    plot.ax.plot(plot.robotAPosition[0], plot.robotAPosition[1], 'bs', label='Robot A')
    plot.ax.plot(plot.robotBPosition[0], plot.robotBPosition[1], 'rs', label='Robot B')

    if aData["rock"]:
        plot.rockCount += 1
        plot.rockAPosition.append({
            'id': plot.rockCount,
            'pos': plot.robotAPosition.copy(),
            'robot': 'A',
            'colour': aData["colour"],
            'size': aData["size"]
        })

    elif aData["cliff"]:
        plot.cliffAPosition.append({'pos': plot.robotAPosition.copy()})

    elif aData["mountain"]:
        plot.mountainAPosition.append({'pos': plot.robotAPosition.copy()})

    if bData["rock"]:
        plot.rockCount += 1
        plot.rockBPosition.append({
            'id': plot.rockCount,
            'pos': plot.robotBPosition.copy(),
            'robot': 'B',
            'colour': bData["colour"],
            'size': bData["size"]
        })

    elif bData["cliff"]:
        plot.cliffBPosition.append({'pos': plot.robotBPosition.copy()})

    elif bData["mountain"]:
        plot.mountainBPosition.append({'pos': plot.robotBPosition.copy()})

    for idx, rock in enumerate(plot.rockAPosition):
        x, y = rock['pos']
        plot.ax.plot(x, y, 'bx', label='Rock A' if idx == 0 else "")
        plot.ax.text(x + 1, y + 1, f"A{rock['id']}", fontsize=8, color='blue')

    for idx, rock in enumerate(plot.rockBPosition):
        x, y = rock['pos']
        plot.ax.plot(x, y, 'rx', label='Rock B' if idx == 0 else "")
        plot.ax.text(x + 1, y + 1, f"B{rock['id']}", fontsize=8, color='red')

    for idx, cliff in enumerate(plot.cliffAPosition):
        x, y = cliff['pos']
        plot.ax.plot(x, y, 'vk', ms=10, mec='b', label='Cliff A' if idx == 0 else "")

    for idx, cliff in enumerate(plot.cliffBPosition):
        x, y = cliff['pos']
        plot.ax.plot(x, y, 'vk', ms=10, mec='r', label='Cliff B' if idx == 0 else "")

    for idx, mountain in enumerate(plot.mountainAPosition):
        x, y = mountain['pos']
        plot.ax.plot(x, y, '^g', ms=10, mec='b', label='Mountain A' if idx == 0 else "")

    for idx, mountain in enumerate(plot.mountainBPosition):
        x, y = mountain['pos']
        plot.ax.plot(x, y, '^g', ms=10, mec='r', label='Mountain B' if idx == 0 else "")

    plot.ax.set_title("Robot Localisation Scope Plot Configuration Map", loc="center", fontsize=10)
    plot.fig.suptitle("RLSPCM", fontsize=18)
    plot.ax.set_xlabel('X-axis')
    plot.ax.set_ylabel('Y-axis')
    plot.ax.legend()

    print(f"Plot Updated")

    all_rocks = plot.rockAPosition + plot.rockBPosition
    all_rocks_sorted = sorted(all_rocks, key=lambda r: r['id'])

    print("\n==== ROCK DATA ====")
    print("Robot\t Number\t Colour\t Size\t\t Coordinates")
    print("-" * 60)
    for rock in all_rocks_sorted:
        x, y = rock['pos']
        print(f"{rock['robot']}\t {rock['id']}\t {rock['colour']}\t {rock['size']}\t\t ({x}, {y})")
    print("=" * 60 + "\n")

    rocks_data = []
    for rock in all_rocks_sorted:
        rocks_data.append({
            "robot": rock['robot'],
            "id": rock['id'],
            "colour": rock['colour'],
            "size": rock['size'],
            "coordinates": rock['pos']
        })

    plot.fig.canvas.draw()
    plot.fig.canvas.flush_events()
    return rocks_data
