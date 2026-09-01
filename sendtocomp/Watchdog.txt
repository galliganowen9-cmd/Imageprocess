from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import time
import os
import subprocess

def wait_until_ready(path):
    size = -1
    while True:
        new_size = os.path.getsize(path)
        if new_size == size:
            return
        size = new_size
        time.sleep(0.2)

class MyHandler(FileSystemEventHandler):
    def on_created(self, event):
        if event.is_directory:
            return

        file_path = event.src_path

        wait_until_ready(file_path)
        print("New file:",file_path)

        subprocess.run([
            "python3",
            "/home/lsf/Imageprocess/Imageprocessdata.py",
            file_path
        ])

    #can do on_created,on_modified,on_deleted, or on_moved

observer = Observer()
observer.schedule(MyHandler(),path = "/home/lsf/fanpov/uploads",recursive = False)
observer.start()

print("watching directory")

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    observer.stop()

observer.join()
