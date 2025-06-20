from multiprocessing import Queue, Process
import aGetData, bGetData, mapping, dumpJson
import time
import json


def main():
    queue = Queue()

    #recieve
    p1 = Process(target=aGetData.getData, args=(queue,))
    p2 = Process(target=bGetData.getData, args=(queue,))

    p1.start()
    p2.start()

    payload = {}
    try:
        while True:
            if not queue.empty():
                key, value = queue.get()
                payload[key] = value
            if "aData" in payload and "bData" in payload:
                break
            time.sleep(0.1)
    #kill
    except KeyboardInterrupt:
        print("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^")
        print("^^^^^^^^^^^^^^^^^^^^^^Terminating process^^^^^^^^^^^^^^^^^^^^^^^^^^^^")
        print("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^")
        raise

    finally:
        if p1.is_alive():
            p1.terminate()
            p1.join()
        if p2.is_alive():
            p2.terminate()
            p2.join()

    aRobotData = payload.get("aData")
    bRobotData = payload.get("bData")

    if aRobotData and bRobotData:
        jsondata = mapping.plot(aRobotData, bRobotData)
        with open("table.json", "w") as outfile:
            json.dump(jsondata, outfile)
        return jsondata
    return None


if __name__ == '__main__':
    JSONDATA = None
    try:
        while True:
            JSONDATA = main()
    except KeyboardInterrupt:
        print("dumping JSON data and exiting...")
        if JSONDATA:
            dumpJson.dump(JSONDATA)
    finally:
        print("Program terminated")
        print('>' * 20)
        SystemExit()
