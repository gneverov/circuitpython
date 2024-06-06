import lvgl
import math


# Demonstrate how scrolling appears automatically
def scroll_1():
    # Create an object with the new style
    panel = lvgl.Object(
        width=200,
        height=200,
        align=lvgl.ALIGN_CENTER,
    )

    child = lvgl.Object(
        panel,
        x=0,
        y=0,
        width=70,
        height=70,
    )
    lvgl.Label(
        child,
        text="Zero",
        align=lvgl.ALIGN_CENTER,
    )

    child = lvgl.Object(
        panel,
        x=160,
        y=80,
        width=80,
        height=80,
    )

    child2 = lvgl.Button(
        child,
        width=100,
        height=50,
    )

    lvgl.Label(
        child2,
        text="Right",
        align=lvgl.ALIGN_CENTER,
    )

    child = lvgl.Object(
        panel,
        x=40,
        y=160,
        width=100,
        height=70,
    )
    lvgl.Label(
        child,
        text="Bottom",
        align=lvgl.ALIGN_CENTER,
    )


# Show an example to scroll snap
def scroll_2():
    def sw_event_cb(e):
        if e.code == lvgl.EVENT_VALUE_CHANGED:
            if sw.state & lvgl.STATE_CHECKED:
                panel.flags |= lvgl.OBJ_FLAG_SCROLL_ONE
            else:
                panel.flags &= ~lvgl.OBJ_FLAG_SCROLL_ONE

    panel = lvgl.Object(
        width=280,
        height=120,
        scroll_snap_x=lvgl.SCROLL_SNAP_CENTER,
        flex_flow=lvgl.FLEX_FLOW_ROW,
        layout=lvgl.LAYOUT_FLEX,
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=20,
    )

    for i in range(10):
        btn = lvgl.Button(panel, width=150, height=lvgl.pct(100))

        label = lvgl.Label(btn)
        if i == 3:
            label.text = f"Panel {i}\nno snap"
            btn.flags &= ~lvgl.OBJ_FLAG_SNAPPABLE
        else:
            label.text = f"Panel {i}"

        label.align = lvgl.ALIGN_CENTER

    panel.update_snap(True)

    # Switch between "One scroll" and "Normal scroll" mode
    sw = lvgl.Switch(align=lvgl.ALIGN_TOP_RIGHT, x=-20, y=10)
    sw.add_event_cb(sw_event_cb, lvgl.EVENT_ALL)
    label = lvgl.Label(text="One scroll")
    label.align_to(sw, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 5)


# Styling the scrollbars
def scroll_4():
    obj = lvgl.Object(
        width=200,
        height=100,
        align=lvgl.ALIGN_CENTER,
    )

    label = lvgl.Label(obj)
    label.text = (
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n"
        "Etiam dictum, tortor vestibulum lacinia laoreet, mi neque consectetur neque, vel mattis odio dolor egestas ligula. \n"
        "Sed vestibulum sapien nulla, id convallis ex porttitor nec. \n"
        "Duis et massa eu libero accumsan faucibus a in arcu. \n"
        "Ut pulvinar odio lorem, vel tempus turpis condimentum quis. Nam consectetur condimentum sem in auctor. \n"
        "Sed nisl augue, venenatis in blandit et, gravida ac tortor. \n"
        "Etiam dapibus elementum suscipit. \n"
        "Proin mollis sollicitudin convallis. \n"
        "Integer dapibus tempus arcu nec viverra. \n"
        "Donec molestie nulla enim, eu interdum velit placerat quis. \n"
        "Donec id efficitur risus, at molestie turpis. \n"
        "Suspendisse vestibulum consectetur nunc ut commodo. \n"
        "Fusce molestie rhoncus nisi sit amet tincidunt. \n"
        "Suspendisse a nunc ut magna ornare volutpat."
    )

    # Remove the style of scrollbar to have clean start
    obj.remove_style(None, lvgl.PART_SCROLLBAR | lvgl.STATE_ANY)

    # Create a transition the animate the some properties on state change
    props = ["bg_opa", "width"]
    trans = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.LINEAR, 200, 0)

    # Create a style for the scrollbars
    style = lvgl.Style(
        width=4,  # Width of the scrollbar
        pad_right=5,  # Space from the parallel side
        pad_top=5,  # Space from the perpendicular side
        radius=2,
        bg_opa=lvgl.OPA_70,
        bg_color=lvgl.Palette.BLUE.main(),
        border_color=lvgl.Palette.BLUE.darken(3),
        border_width=2,
        shadow_width=8,
        shadow_spread=2,
        shadow_color=lvgl.Palette.BLUE.darken(1),
        transition=trans,
    )

    # Make the scrollbars wider and use 100% opacity when scrolled
    style_scrolled = lvgl.Style(
        width=8,
        bg_opa=lvgl.OPA_COVER,
    )

    obj.add_style(style, lvgl.PART_SCROLLBAR)
    obj.add_style(style_scrolled, lvgl.PART_SCROLLBAR | lvgl.STATE_SCROLLED)


# Scrolling with Right To Left base direction
def scroll_5():
    if hasattr(lvgl.Font, "DEJAVU_16_PERSIAN_HEBREW"):
        obj = lvgl.Object(
            base_dir=lvgl.BASE_DIR_RTL,
            width=200,
            height=100,
            align=lvgl.ALIGN_CENTER,
        )

        lvgl.Label(
            obj,
            text="میکروکُنترولر (به انگلیسی: Microcontroller) گونه‌ای ریزپردازنده است که دارای حافظهٔ دسترسی تصادفی (RAM) و حافظهٔ فقط‌خواندنی (ROM)، تایمر، پورت‌های ورودی و خروجی (I/O) و درگاه ترتیبی (Serial Port پورت سریال)، درون خود تراشه است، و می‌تواند به تنهایی ابزارهای دیگر را کنترل کند. به عبارت دیگر یک میکروکنترلر، مدار مجتمع کوچکی است که از یک CPU کوچک و اجزای دیگری مانند تایمر، درگاه‌های ورودی و خروجی آنالوگ و دیجیتال و حافظه تشکیل شده‌است.",
            width=400,
            font=lvgl.Font.DEJAVU_16_PERSIAN_HEBREW,
        )
    else:
        print("missing font DEJAVU_16_PERSIAN_HEBREW")


# Translate the object as they scroll
def scroll_6():
    def scroll_event_cb(e):
        cont_a = cont.coords
        cont_y_center = (cont_a.y1 + cont_a.y2) // 2

        r = (cont_a.y2 - cont_a.y1) * 7 // 10
        for child in cont.children:
            child_a = child.coords

            child_y_center = (child_a.y1 + child_a.y2) // 2

            diff_y = child_y_center - cont_y_center
            diff_y = abs(diff_y)

            # Get the x of diff_y on a circle.
            # If diff_y is out of the circle use the last point of the circle (the radius)
            if diff_y >= r:
                x = r
            else:
                # Use Pythagoras theorem to get x from radius and y
                x = r - int(math.sqrt(r * r - diff_y * diff_y))

            # Translate the item by the calculated X coordinate
            child.translate_x = x

            # Use some opacity with larger translations
            child.opa = x * (lvgl.OPA_TRANSP - lvgl.OPA_COVER) // r + lvgl.OPA_COVER

    cont = lvgl.Object(
        width=200,
        height=200,
        align=lvgl.ALIGN_CENTER,
        flex_flow=lvgl.FLEX_FLOW_COLUMN,
        layout=lvgl.LAYOUT_FLEX,
        radius=lvgl.RADIUS_CIRCLE,
        clip_corner=True,
        scroll_dir=lvgl.DIR_VER,
        scroll_snap_y=lvgl.SCROLL_SNAP_CENTER,
        scrollbar_mode=lvgl.SCROLLBAR_MODE_OFF,
    )
    cont.add_event_cb(scroll_event_cb, lvgl.EVENT_SCROLL)

    for i in range(20):
        btn = lvgl.Button(cont, width=lvgl.pct(100))
        lvgl.Label(btn, text=f"Button {i}")

    # Update the buttons position manually for first
    cont.send_event(lvgl.EVENT_SCROLL)

    # Be sure the fist button is in the middle
    cont.children[0].scroll_to_view(False)
