import lvgl


# Using the Size, Position and Padding style properties
def style_1():
    style = lvgl.Style(
        radius=5,
        # Make a gradient
        width=150,
        height=lvgl.SIZE_CONTENT,
        pad_top=20,
        pad_bottom=20,
        pad_left=5,
        x=lvgl.pct(50),
        y=80,
    )

    # Create an object with the new style
    obj = lvgl.Object()
    obj.add_style(style)

    lvgl.Label(obj, text="Hello")


# Creating a transition
def style_10():
    props = ["bg_color", "border_color", "border_width"]

    # A default transition
    # Make it fast (100ms) and start with some delay (200 ms)
    trans_def = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.LINEAR, 100, 200)

    # A special transition when going to pressed state
    # Make it slow (500 ms) but start  without delay
    trans_pr = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.LINEAR, 500, 0)

    style_def = lvgl.Style(transition=trans_def)

    style_pr = lvgl.Style(
        bg_color=lvgl.Palette.RED.main(),
        border_width=6,
        border_color=lvgl.Palette.RED.darken(3),
        transition=trans_pr,
    )

    # Create an object with the new style_pr
    obj = lvgl.Object()
    obj.add_style(style_def)
    obj.add_style(style_pr, lvgl.STATE_PRESSED)

    obj.align = lvgl.ALIGN_CENTER


# Opacity and Transformations
def style_15():
    # Normal button
    btn = lvgl.Button(
        width=100,
        height=40,
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=-70,
    )

    lvgl.Label(
        btn,
        text="Normal",
        align=lvgl.ALIGN_CENTER,
    )

    # *Set opacity
    # The button and the label is rendered to a layer first and that layer is blended
    btn = lvgl.Button(
        width=100,
        height=40,
        opa=lvgl.OPA_50,
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=0,
    )

    lvgl.Label(
        btn,
        text="Opa:50%",
        align=lvgl.ALIGN_CENTER,
    )

    # Set transformations
    # The button and the label is rendered to a layer first and that layer is transformed
    btn = lvgl.Button(
        width=100,
        height=40,
        transform_rotation=150,  # 15 deg
        transform_scale_x=256 + 64,  # 1.25x
        transform_scale_y=256 + 64,
        transform_pivot_x=50,
        transform_pivot_y=20,
        opa=lvgl.OPA_50,
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=70,
    )

    lvgl.Label(
        btn,
        text="Transf.",
        align=lvgl.ALIGN_CENTER,
    )
