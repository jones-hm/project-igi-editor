import pyautogui
import time
import pygetwindow as gw
import subprocess
import os

# Disable failsafe to prevent early termination
pyautogui.FAILSAFE = False

def run_test():
    exe_path = r'bin\Debug\igi-editor.exe'
    if not os.path.exists(exe_path):
        print(f"Error: {exe_path} not found")
        return

    print("Starting IGI Editor...")
    # Move camera closer to Watertower (24658918, -55966376)
    proc = subprocess.Popen([exe_path, '-level', '1', '-pos', '24650000', '-55960000', '174420128'])
    time.sleep(15) # Wait for load

    windows = gw.getWindowsWithTitle('IGI')
    if not windows:
        print("Window not found")
        proc.terminate()
        return

    win = windows[0]
    try:
        win.activate()
    except Exception as e:
        print("Could not activate:", e)
    
    time.sleep(2)
    print("Pressing F4...")
    pyautogui.press('f4')
    time.sleep(1)
    
    center_x = win.left + win.width // 2
    center_y = win.top + win.height // 2

    print("Beginning 360 degree search...")
    # Rotate 360 degrees to find objects
    # 1600 pixels is roughly 360 in many GLUT apps
    for i in range(40):
        pyautogui.moveRel(40, 0, duration=0.1)
        shot_path = f'screenshot_360_{i:02d}.png'
        pyautogui.screenshot(shot_path)
        print(f"[{i}/40] Saved {shot_path}")
        time.sleep(0.05)

        
    print("Test complete. Terminating app...")
    proc.terminate()

if __name__ == "__main__":
    run_test()
