import lvgl
from . import clear_screen


def get_started_1():
    clear_screen()
    lvgl.display.screen.bg_color = 0x003A57
    lvgl.Label(text="Hello world", text_color=0xFFFFFF, align=lvgl.ALIGN_CENTER)
    lvgl.run_forever()


def get_started_2():
    clear_screen()
    btn = lvgl.Button(x=10, y=10, width=120, height=50)
    lbl = lvgl.Label(btn, text="Button", align=lvgl.ALIGN_CENTER)

    btn.count = 0

    def btn_event_cb(e):
        if e.code == lvgl.EVENT_CLICKED:
            btn.count += 1
            lbl.text = f"Button: {btn.count}"

    btn.add_event(btn_event_cb, lvgl.EVENT_ALL)
    lvgl.run_forever()


def get_started_4():
    clear_screen()
    slider = lvgl.Slider(width=200, align=lvgl.ALIGN_CENTER)
    label = lvgl.Label(text="0")
    label.align_to(slider, lvgl.ALIGN_OUT_TOP_MID, 0, -15)

    def slider_event_cb(e):
        label.text = str(slider.value)

    slider.add_event(slider_event_cb, lvgl.EVENT_VALUE_CHANGED)
    lvgl.run_forever()
