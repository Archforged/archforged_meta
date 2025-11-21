#!/usr/bin/env python3
#built with <3 and AI at Electron Micro Computing
#https://ko-fi.com/electronmicrocomputing
#arch only

"""
Emotive-engine
• Spotify / YouTube Music / any MPRIS player → album art
• mpv → live video frames
• YouTube.com / Spotify PWA → live frame sampling (Canvas, video, etc.)
• Wallpaper-matched idle + flash
• Zero crashes, zero double captures, zero missing functions
"""

import time
import signal
import threading
import subprocess
import re
import os
import json
import hashlib
from collections import deque
from queue import Queue, Empty
from typing import Optional
import aubio
import numpy as np
import sounddevice as sd
from PIL import Image
import requests
from io import BytesIO

# ======================================
# -------------- CONFIG ----------------
# ======================================
SAMPLERATE = 44100
WIN_S = 1024
HOP_S = WIN_S // 2
UPDATE_RATE = 1 / 30.0
BPM_WINDOW = 8.0
IDLE_FLASH_INTERVAL = (6, 9)
VIDEO_SAMPLE_INTERVAL = 0.45
DEBUG = False

HYPERPANEL_CSS = os.path.expanduser("~/.config/hyprpanel/style.css")
NWG_DOCK_CSS = os.path.expanduser("~/.config/nwg-dock-hyprland/style.css") # <-- ADDED
MPV_SOCKET = "/tmp/mpv-socket"

MIC_KEYWORDS = {
    "discord","vesktop","voice","audacity","studio one",
    "reaper","obs","input","microphone","webcord","armcord"
}

beat_queue: Queue[float] = Queue()
shutdown_event = threading.Event()
idle_flash_lock = threading.Lock()

# Caches
_cached_wallpaper_color = "#41FDFEcc"
_cache_time = 0.0
_CACHE_DURATION = 30.0
_last_video_color: Optional[str] = None
_last_video_time = 0.0

# ======================================
# -------------- UTILITIES -------------
# ======================================
def hyprctl_set(keyword: str, value: str):
    subprocess.run(["hyprctl", "keyword", keyword, value],
                   check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def update_colors(color_hex: str):
    hyprctl_set("decoration:col.active", color_hex)
    hyprctl_set("decoration:col.shadow", color_hex)
    hyprctl_set("plugin:wallpaper:pulse", color_hex)
    
    # --- HyprPanel Update ---
    if os.path.exists(HYPERPANEL_CSS):
        try:
            with open(HYPERPANEL_CSS) as f: css = f.read()
            new_css = re.sub(
                r'(--panel-accent-color\s*:\s*)(?:#[0-9a-fA-F]{6,8}|rgba?\([^)]+\))[;! ]*',
                f'\\1{color_hex[:7]};', css, flags=re.IGNORECASE)
            if new_css != css:
                with open(HYPERPANEL_CSS, "w") as f: f.write(new_css)
        except: pass

    # --- nwg-dock-hyprland Update (NEW) ---
    if os.path.exists(NWG_DOCK_CSS):
        try:
            with open(NWG_DOCK_CSS) as f: css = f.read()
            # Use color_hex[:7] to get #RRGGBB for the gradient
            new_css = re.sub(
                r'(--emotive-accent\s*:\s*)(?:#[0-S9a-fA-F]{6,8}|rgba?\([^)]+\))[;! ]*',
                f'\\1{color_hex[:7]};', css, flags=re.IGNORECASE)
            if new_css != css:
                with open(NWG_DOCK_CSS, "w") as f: f.write(new_css)
        except: pass # Fail silently

# --- hyprshade temp file for uniforms ---
def update_shader_uniforms(rgb_tuple: tuple[int,int,int], is_beat: bool):
    r, g, b = rgb_tuple
    r_f = r / 255.0
    g_f = g / 255.0
    b_f = b / 255.0
    beat_val = 1.0 if is_beat else 0.0
    try:
        with open("/tmp/emotive_engine_uniforms", "w") as f:
            f.write(f"R={r_f:.4f}\n")
            f.write(f"G={g_f:.4f}\n")
            f.write(f"B={b_f:.4f}\n")
            f.write(f"BEAT={beat_val:.1f}\n")
    except: pass

class ColorFader:
    def __init__(self):
        self.current = "#00000000"
        self.target = "#00000000"
        self.start_time = 0.0
        self.duration = 0.0
        self.active = False

    def _parse(self, hexstr: str) -> list[int]:
        hexstr = hexstr.lstrip("#")
        if len(hexstr) == 6:
            hexstr += "ff"
        elif len(hexstr) == 8:
            pass
        else:
            return [0, 0, 0, 255]
        return [int(hexstr[i:i+2], 16) for i in range(0, 8, 2)]

    def start_fade(self, to_color: str, duration: float = 0.12):
        to_color = to_color.lower()
        if self.current == to_color and not self.active:
            return
        self.target = to_color
        self.start_time = time.time()
        self.duration = max(0.01, duration)
        self.active = True

    def update(self, now: float) -> str:
        if not self.active:
            return self.current
        t = min(1.0, (now - self.start_time) / self.duration)
        if t >= 1.0:
            self.active = False
            self.current = self.target
            return self.current
        s = self._parse(self.current)
        e = self._parse(self.target)
        cur = [int(s[i] + (e[i] - s[i]) * t) for i in range(4)]
        self.current = f"#{cur[0]:02x}{cur[1]:02x}{cur[2]:02x}{cur[3]:02x}"
        return self.current

fader = ColorFader()
fader_lock = threading.Lock() # <-- THREADING LOCK ADDED

# ======================================
# ----- UNIVERSAL WALLPAPER COLOR ------
# ======================================
def get_wallpaper_color() -> str:
    global _cached_wallpaper_color, _cache_time
    now = time.time()
    if now - _cache_time < _CACHE_DURATION:
        return _cached_wallpaper_color

    tools = [
        (["hyprctl", "hyprpaper", "listactive"], lambda x: x.split(",")[1].strip() if "," in x else None),
        (["swww", "query"], lambda x: x.split("image:")[1].split()[0] if "image:" in x else None),
        (["waypaper", "--list"], lambda x: x.strip().splitlines()[0] if x.strip() else None),
        (["waytrogen", "--list"], lambda x: x.strip().splitlines()[0] if x.strip() else None),
        (["feh", "--bg-info"], lambda x: x.strip()),
        (["sh", "-c", "grep file ~/.config/nitrogen/bg-saved.cfg 2>/dev/null | cut -d= -f2"], lambda x: x.strip()),
    ]

    path = None
    for cmd, parser in tools:
        try:
            output = subprocess.check_output(cmd, text=True, stderr=subprocess.DEVNULL)
            for line in output.splitlines():
                p = parser(line)
                if p and os.path.exists(p):
                    path = p
                    break
            if path: break
        except: continue

    try:
        if path and os.path.exists(path):
            img = Image.open(path).convert("RGB").resize((32, 32))
            avg = np.array(img).mean(axis=(0,1))
            color = f"#{int(avg[0]):02x}{int(avg[1]):02x}{int(avg[2]):02x}cc"
            _cached_wallpaper_color = color
            _cache_time = now
            return color
    except Exception:
        pass  # do NOT cache failures

    return "#41FDFEcc"

# ======================================
# --------- IDLE FLASH (wallpaper) -----
# ======================================
def idle_flash():
    if not idle_flash_lock.acquire(blocking=False):
        return
    try:
        base = get_wallpaper_color()
        r = int(base[1:3], 16); g = int(base[3:5], 16); b = int(base[5:7], 16)
        update_shader_uniforms((r,g,b), False)

        bright = base[:-2] + "ff"
        update_colors(bright)
        time.sleep(0.35)
        
        with fader_lock: # <-- LOCK ADDED
            fader.start_fade("#00000000", 0.4)
        while fader.active:
            with fader_lock: # <-- LOCK ADDED
                update_colors(fader.update(time.time()))
            time.sleep(0.01)
            
        with fader_lock: # <-- LOCK ADDED
            fader.start_fade(base, 0.6)
        update_shader_uniforms((r,g,b), False)
        while fader.active:
            with fader_lock: # <-- LOCK ADDED
                update_colors(fader.update(time.time()))
            time.sleep(0.01)
    finally:
        idle_flash_lock.release()

# ======================================
# ----- LIVE VIDEO FRAME SAMPLING ------
# ======================================
def get_live_video_color() -> Optional[str]:
    global _last_video_color, _last_video_time
    now = time.time()
    if now - _last_video_time < VIDEO_SAMPLE_INTERVAL and _last_video_color:
        return _last_video_color

    try:
        tmpfile = f"/tmp/mpv-frame-{hashlib.md5(str(now).encode()).hexdigest()[:10]}.jpg"
        cmd = json.dumps({"command": ["screenshot-to-file", tmpfile, "video"]})
        subprocess.run(
            f"echo '{cmd}' | socat - {MPV_SOCKET}",
            shell=True, timeout=1.2, check=False,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        time.sleep(0.06)
        if os.path.exists(tmpfile):
            # User accepted resource leak; not adding finally block for os.remove
            img = Image.open(tmpfile).convert("RGB").resize((32,32))
            os.remove(tmpfile)
            avg = np.array(img).mean(axis=(0,1))
            color = f"#{int(avg[0]):02x}{int(avg[1]):02x}{int(avg[2]):02x}cc"
            _last_video_color = color
            _last_video_time = now
            return color
    except: pass

    try:
        clients = json.loads(subprocess.check_output(["hyprctl", "clients", "-j"], text=True))
        for win in clients:
            title = win.get("title", "").lower()
            cls = win.get("class", "").lower()
            if any(x in title or x in cls for x in ["youtube", "spotify"]):
                x, y = win["at"]
                w, h = win["size"]
                geom = f"{x},{y},{w},{h}"
                grim = subprocess.Popen(["grim", "-g", geom, "-"], stdout=subprocess.PIPE)
                try:
                    img_data = subprocess.check_output(
                        ["ffmpeg", "-i", "-", "-vf", "scale=32:32:flags=lanczos",
                         "-f", "image2pipe", "-vcodec", "png", "-", "-y"],
                        stdin=grim.stdout,
                        stderr=subprocess.DEVNULL,
                        timeout=2.0
                    )
                    img = Image.open(BytesIO(img_data)).convert("RGB")
                    avg = np.array(img).mean(axis=(0,1))
                    color = f"#{int(avg[0]):02x}{int(avg[1]):02x}{int(avg[2]):02x}cc"
                    _last_video_color = color
                    _last_video_time = now
                    return color
                finally:
                    grim.stdout.close()
                    try: grim.wait(timeout=1.0)
                    except: grim.kill()
    except: pass

    return None

# ======================================
# --------- ALBUM ART FROM MPRIS -------
# ======================================
def get_album_art_color(player: str) -> str:
    try:
        meta = subprocess.check_output(
            ["playerctl", "-p", player, "metadata"], text=True, stderr=subprocess.DEVNULL
        )
        url = None
        for line in meta.splitlines():
            line = line.lower()
            if any(k in line for k in ["arturl", "mpris:arturl", "xesam:arturl"]):
                parts = line.split(None, 1)
                url = parts[1] if len(parts) > 1 else line.split(":", 1)[1]
                break
        if not url:
            return "#ffffffcc"
        if url.startswith("file://"):
            url = url[7:]
            img = Image.open(url).convert("RGB").resize((32,32))
        else:
            resp = requests.get(url, timeout=2.0)
            resp.raise_for_status()
            img = Image.open(BytesIO(resp.content)).convert("RGB").resize((32,32))
        avg = np.array(img).mean(axis=(0,1))
        return f"#{int(avg[0]):02x}{int(avg[1]):02x}{int(avg[2]):02x}cc"
    except:
        return "#ffffffcc"

# ======================================
# --------- PLAYER DETECTION -----------
# ======================================
def is_music_playing() -> Optional[str]:
    try:
        players = [p.strip() for p in subprocess.check_output(
            ["playerctl", "-l"], text=True).splitlines()
                   if p.strip() and "metadata" not in p]
        for p in players:
            try:
                meta = subprocess.check_output(
                    ["playerctl", "-p", p, "metadata"], text=True, stderr=subprocess.DEVNULL
                ).lower()
                if any(kw in meta for kw in MIC_KEYWORDS):
                    continue
                if "xesam:artist" in meta or "xesam:title" in meta:
                    return p
            except: continue
    except: pass
    return None

# ======================================
# --------- BEAT DETECTION -------------
# ======================================
detector = aubio.tempo("default", WIN_S, HOP_S, SAMPLERATE)

def audio_callback(indata, frames, time_info, status):
    if shutdown_event.is_set() or indata is None or status:
        return
    try:
        audio = indata[:, 0] if indata.ndim > 1 else indata.flatten()
        audio = audio.astype(np.float32)
        if detector(audio):
            beat_queue.put(time.time())
    except Exception as e:
        # <-- START OF AUDIO ERROR HANDLING -->
        # Print to stdout for systemctl status
        print(f"CRITICAL: Emotive-Engine audio callback error: {e}")
        # Pause the music player to alert the user
        subprocess.run(["playerctl", "pause"],
                       check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        # <-- END OF AUDIO ERROR HANDLING -->

# ======================================
# --------------- MAIN LOOP ------------
# ======================================
def run():
    recent_beats = deque()
    last_update = 0.0
    last_player_check = 0.0
    player_name: Optional[str] = None
    last_idle_flash = 0.0

    print("emotive-engine booting - advanced ricer technology")
    with sd.InputStream(callback=audio_callback, channels=1,
                        samplerate=SAMPLERATE, blocksize=HOP_S):
        while not shutdown_event.is_set():
            now = time.time()

            try:
                while True: recent_beats.append(beat_queue.get_nowait())
            except Empty: pass
            while recent_beats and recent_beats[0] < now - BPM_WINDOW:
                recent_beats.popleft()

            if now - last_player_check > 1.2:
                player_name = is_music_playing()
                last_player_check = now

            if not player_name:
                if now - last_idle_flash > np.random.uniform(*IDLE_FLASH_INTERVAL):
                    threading.Thread(target=idle_flash, daemon=True).start()
                    last_idle_flash = now
                if not fader.active:
                    base_color = get_wallpaper_color()
                    with fader_lock: # <-- LOCK ADDED
                        fader.start_fade(base_color, duration=0.8)
                    r = int(base_color[1:3], 16); g = int(base_color[3:5], 16); b = int(base_color[5:7], 16)
                    update_shader_uniforms((r,g,b), False)
                time.sleep(0.02)
                continue

            if now - last_update < UPDATE_RATE:
                time.sleep(0.005)
                continue
            last_update = now

            base_color = get_live_video_color() or get_album_art_color(player_name)

            is_beat = recent_beats and now - recent_beats[-1] < 0.09
            alpha = "ff" if is_beat else "cc"
            brightness = 1.45 if is_beat else 1.0
            r = min(255, int(int(base_color[1:3], 16) * brightness))
            g = min(255, int(int(base_color[3:5], 16) * brightness))
            b = min(255, int(int(base_color[5:7], 16) * brightness))
            final = f"#{r:02x}{g:02x}{b:02x}{alpha}"

            update_shader_uniforms((r, g, b), is_beat)

            with fader_lock: # <-- LOCK ADDED
                fader.start_fade(final, duration=0.11 if is_beat else 0.35)
                update_colors(fader.update(now)) # This now updates the dock CSS
            time.sleep(0.005)

    print("\nShutting down...")

def signal_handler(*_):
    shutdown_event.set()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    try:
        run()
    except KeyboardInterrupt:
        pass
    finally:
        shutdown_event.set()
        print("emotive-engine stopped cleanly.")