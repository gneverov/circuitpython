import lvgl


def arc_1():
    def value_changed_event_cb(e):
        label.text = f"{arc.value}%"

        # Rotate the label to the current position of the arc
        arc.rotate_obj_to_angle(label, 25)

    label = lvgl.Label()

    # Create an Arc
    arc = lvgl.Arc(
        width=150,
        height=150,
        rotation=135,
        bg_start_angle=0,
        bg_end_angle=270,
        value=10,
        align=lvgl.ALIGN_CENTER,
    )
    arc.add_event_cb(value_changed_event_cb, lvgl.EVENT_VALUE_CHANGED)

    # Manually update the label for the first time
    arc.send_event(lvgl.EVENT_VALUE_CHANGED)


# Create an arc which acts as a loader.
def arc_2():
    def set_angle(a, v):
        arc.value = v

    # Create an Arc
    arc = lvgl.Arc(
        rotation=270,
        bg_start_angle=0,
        bg_end_angle=360,
    )
    arc.remove_style(None, lvgl.PART_KNOB)  # Be sure the knob is not displayed
    arc.flags &= ~lvgl.OBJ_FLAG_CLICKABLE  # To not allow adjusting by click
    arc.align = lvgl.ALIGN_CENTER

    lvgl.Anim(
        var=arc,
        exec_cb=set_angle,
        duration=1000,
        repeat_count=lvgl.ANIM_REPEAT_INFINITE,  # Just for the demo
        repeat_delay=500,
        start_value=0,
        end_value=100,
    ).start()
