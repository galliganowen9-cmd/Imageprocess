from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import time
import os
import subprocess
UPLOAD_DIR = "/home/lsf/fanpov/uploads"
PROCESS_DIR = "/home/lsf/Imageprocess/headerfiles"
def wait_until_ready(path):
    size = -1
    while True:
        new_size = os.path.getsize(path)
        if new_size == size:
            return
        size = new_size
        time.sleep(0.2)

class ImageHandler(FileSystemEventHandler):
    def on_created(self, event):
        if event.is_directory:
            return
        filename = os.path.basename(event.src_path)
        file_path = event.src_path

        wait_until_ready(file_path)
        print("New file:",file_path)

        subprocess.run([
            "python3",
            "/home/lsf/Imageprocess/Imageprocessdata.py",
            file_path
        ])
        
            

    #can do on_created,on_modified,on_deleted, or on_moved
class HeaderHandler(FileSystemEventHandler):
    def on_closed(self,event):
        if event.is_directory:
            return
        filename = os.path.basename(event.src_path)
        file_path = event.src_path
        if not filename.endswith(".h"):
            return
        try:
            subprocess.run([
                "python3",
                "/home/lsf/Imageprocess/bridge.py",
                file_path
            ], cwd = PROCESS_DIR, check = True)
        except subprocess.CalledProcessError as e:
            print("Transmission failed", e, flush = True)
image_observer = Observer()
header_observer= Observer()



image_observer.schedule(ImageHandler(),path = UPLOAD_DIR,recursive = False)
header_observer.schedule(HeaderHandler(),path = PROCESS_DIR,recursive = False)
image_observer.start()
header_observer.start()

print("watching directory")

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    pass
finally:
    image_observer.stop()
    header_observer.stop()
image_observer.join()
header_observer.join()
