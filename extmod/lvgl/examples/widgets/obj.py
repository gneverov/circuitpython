import lvgl


def obj_1():
    lvgl.Object(
        width=100,
        height=50,
        align=lvgl.ALIGN_CENTER,
        x=-60,
        y=-30,
    )

    style_shadow = lvgl.Style(
        shadow_width=10,
        shadow_spread=5,
        shadow_color=lvgl.Palette.BLUE.main(),
    )

    obj2 = lvgl.Object()
    obj2.add_style(style_shadow)
    obj2.align_as(lvgl.ALIGN_CENTER, 60, 30)


# Make an object draggable.
def obj_2():
    def drag_event_handler(e):
        indev = lvgl.indev
        if not indev:
            return

        vect = indev.get_vect()
        nonlocal obj
        obj.x += vect.x
        obj.y += vect.y

    obj = lvgl.Object(
        width=150,
        height=100,
    )
    obj.add_event_cb(drag_event_handler, lvgl.EVENT_PRESSING)

    lvgl.Label(
        obj,
        text="Drag me",
        align=lvgl.ALIGN_CENTER,
    )
