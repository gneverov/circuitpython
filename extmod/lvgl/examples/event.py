import lvgl


# Add click event to a button
def event_1():
    cnt = 1

    def event_cb(e):
        print("Clicked")

        nonlocal cnt
        label.text = str(cnt)
        cnt += 1

    btn = lvgl.Button(
        width=100,
        height=50,
        align=lvgl.ALIGN_CENTER,
    )
    btn.add_event_cb(event_cb, lvgl.EVENT_CLICKED)

    label = lvgl.Label(
        btn,
        text="Click me!",
        align=lvgl.ALIGN_CENTER,
    )


# Handle multiple events
def event_2():
    def event_cb(e):
        if e.code == lvgl.EVENT_PRESSED:
            label.text = "The last button event:\nLV_EVENT_PRESSED"
        elif e.code == lvgl.EVENT_CLICKED:
            label.text = "The last button event:\nLV_EVENT_CLICKED"
        elif e.code == lvgl.EVENT_LONG_PRESSED:
            label.text = "The last button event:\nLV_EVENT_LONG_PRESSED"
        elif e.code == lvgl.EVENT_LONG_PRESSED_REPEAT:
            label.text = "The last button event:\nLV_EVENT_LONG_PRESSED_REPEAT"

    btn = lvgl.Button(
        width=100,
        height=50,
        align=lvgl.ALIGN_CENTER,
    )

    lvgl.Label(
        btn,
        text="Click me!",
        align=lvgl.ALIGN_CENTER,
    )

    label = lvgl.Label(text="The last button event:\nNone")

    btn.add_event_cb(event_cb, lvgl.EVENT_ALL)


# Demonstrate event bubbling
def event_3():
    def event_cb(e):
        # The original target of the event. Can be the buttons or the container*/
        target = e.target

        # The current target is always the container as the event is added to it*/
        cont = e.current_target

        # If container was clicked do nothing*/
        if target is cont:
            return

        # Make the clicked buttons red
        target.bg_color = lvgl.Palette.RED.main()

    cont = lvgl.Object(
        width=290,
        height=200,
        align=lvgl.ALIGN_CENTER,
        flex_flow=lvgl.FLEX_FLOW_ROW_WRAP,
        layout=lvgl.LAYOUT_FLEX,
    )

    for i in range(30):
        btn = lvgl.Button(
            cont,
            width=70,
            height=50,
        )
        btn.flags |= lvgl.OBJ_FLAG_EVENT_BUBBLE

        lvgl.Label(
            btn,
            text=str(i),
            align=lvgl.ALIGN_CENTER,
        )

    cont.add_event_cb(event_cb, lvgl.EVENT_CLICKED)
