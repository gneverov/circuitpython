import lvgl


def switch_1():
    def event_handler(e):
        if e.code == lvgl.EVENT_VALUE_CHANGED:
            print(f"State: {'On' if e.target.state & lvgl.STATE_CHECKED else 'Off'}")

    lvgl.screen.update(
        flex_flow=lvgl.FLEX_FLOW_COLUMN,
        flex_main_place=lvgl.FLEX_ALIGN_CENTER,
        flex_cross_place=lvgl.FLEX_ALIGN_CENTER,
        flex_track_place=lvgl.FLEX_ALIGN_CENTER,
        layout=lvgl.LAYOUT_FLEX,
    )

    sw = lvgl.Switch()
    sw.add_event_cb(event_handler, lvgl.EVENT_ALL)
    sw.flags |= lvgl.OBJ_FLAG_EVENT_BUBBLE

    sw = lvgl.Switch()
    sw.state |= lvgl.STATE_CHECKED
    sw.add_event_cb(event_handler, lvgl.EVENT_ALL)

    sw = lvgl.Switch()
    sw.state |= lvgl.STATE_DISABLED
    sw.add_event_cb(event_handler, lvgl.EVENT_ALL)

    sw = lvgl.Switch()
    sw.state |= lvgl.STATE_CHECKED | lvgl.STATE_DISABLED
    sw.add_event_cb(event_handler, lvgl.EVENT_ALL)
