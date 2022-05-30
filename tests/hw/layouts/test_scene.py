import time
from leddite.hw.screens import VirtualScreen
from leddite.hw.layouts import Scene

def test_scene_tracks_shift():
    s_height= 16
    s_width = 16
    virtual_screen = VirtualScreen(s_height, s_width, True)
    scene = Scene(screen=virtual_screen, content_height=s_height, inter_track_space=0)
    scene.generate_tracks()
    content = [ virtual_screen.color((255, 255, 255)) for _ in range(s_width * s_height) ]
    scene.tracks[0].add_content(content)
    for i in range(s_width):
        scene.frame()
        for track in scene.tracks:
            print(track.horizontal_shift)
        scene.shift_tracks_horizontal(1)
        time.sleep(0.15)

tests = [
 test_scene_tracks_shift
]
 
def main():
    for test in tests:
        print("Name:", test.__name__)
        test()

if __name__ == '__main__':
    main()
