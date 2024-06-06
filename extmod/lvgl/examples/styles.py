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


# Using the background style properties
def style_2():
    style = lvgl.Style(
        radius=5,
        # Make a gradient
        bg_opa=lvgl.OPA_COVER,
        bg_grad=lvgl.GradDsc(
            dir=lvgl.GRAD_DIR_VER,
            stops=[
                # Shift the gradient to the bottom
                (lvgl.Palette.GREY.lighten(1), lvgl.OPA_COVER, 128),
                (lvgl.Palette.BLUE.main(), lvgl.OPA_COVER, 192),
            ],
        ),
    )

    # Create an object with the new style
    obj = lvgl.Object()
    obj.add_style(style)
    obj.align = lvgl.ALIGN_CENTER


# Using the border style properties
def style_3():
    style = lvgl.Style(
        # Set a background color and a radius
        radius=10,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(1),
        # Add border to the bottom+right
        border_color=lvgl.Palette.BLUE.main(),
        border_width=5,
        border_opa=lvgl.OPA_50,
        border_side=lvgl.BORDER_SIDE_BOTTOM | lvgl.BORDER_SIDE_RIGHT,
    )

    # Create an object with the new style
    obj = lvgl.Object()
    obj.add_style(style)
    obj.align = lvgl.ALIGN_CENTER


# Using the outline style properties
def style_4():
    style = lvgl.Style(
        # Set a background color and a radius
        radius=5,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(1),
        # Add outline
        outline_width=2,
        outline_color=lvgl.Palette.BLUE.main(),
        outline_pad=8,
    )

    # Create an object with the new style
    obj = lvgl.Object()
    obj.add_style(style)
    obj.align = lvgl.ALIGN_CENTER


# Using the Shadow style properties
def style_5():
    style = lvgl.Style(
        # Set a background color and a radius
        radius=5,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(1),
        # Add a shadow
        shadow_width=55,
        shadow_color=lvgl.Palette.BLUE.main(),
    )

    # Create an object with the new style
    obj = lvgl.Object()
    obj.add_style(style)
    obj.align = lvgl.ALIGN_CENTER


# Using the Image style properties
def style_6():
    style = lvgl.Style(
        # Set a background color and a radius
        radius=5,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(3),
        border_width=2,
        border_color=lvgl.Palette.BLUE.main(),
        image_recolor=lvgl.Palette.BLUE.main(),
        image_recolor_opa=lvgl.OPA_50,
        transform_rotation=300,
    )

    # Create an object with the new style
    obj = lvgl.Image()
    obj.add_style(style)

    from . import get_asset

    obj.src = get_asset("img_cogwheel_argb.bin")

    obj.align = lvgl.ALIGN_CENTER


# Using the Arc style properties
def style_7():
    style = lvgl.Style(
        arc_color=lvgl.Palette.RED.main(),
        arc_width=4,
    )

    # Create an object with the new style
    obj = lvgl.Arc()
    obj.add_style(style)
    obj.align = lvgl.ALIGN_CENTER


# Using the text style properties
def style_8():
    style = lvgl.Style(
        radius=5,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(2),
        border_width=2,
        border_color=lvgl.Palette.BLUE.main(),
        pad_top=10,
        pad_bottom=10,
        pad_left=10,
        pad_right=10,
        text_color=lvgl.Palette.BLUE.main(),
        text_letter_space=5,
        text_line_space=20,
        text_decor=lvgl.TEXT_DECOR_UNDERLINE,
    )

    # Create an object with the new style
    obj = lvgl.Label()
    obj.add_style(style)
    obj.text = "Text of\na label"

    obj.align = lvgl.ALIGN_CENTER


# Using the line style properties
def style_9():
    style = lvgl.Style(
        line_color=lvgl.Palette.GREY.main(),
        line_width=6,
        line_rounded=True,
    )

    # Create an object with the new style
    obj = lvgl.Line()
    obj.add_style(style)

    obj.points = [(10, 30), (30, 50), (100, 0)]

    obj.align = lvgl.ALIGN_CENTER


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


# Using multiple styles
def style_11():
    # A base style
    style_base = lvgl.Style(
        bg_color=lvgl.Palette.LIGHT_BLUE.main(),
        border_color=lvgl.Palette.LIGHT_BLUE.darken(3),
        border_width=2,
        radius=10,
        shadow_width=10,
        shadow_offset_y=5,
        shadow_opa=lvgl.OPA_50,
        text_color=lvgl.color_white(),
        width=100,
        height=lvgl.SIZE_CONTENT,
    )

    # Set only the properties that should be different
    style_warning = lvgl.Style(
        bg_color=lvgl.Palette.YELLOW.main(),
        border_color=lvgl.Palette.YELLOW.darken(3),
        text_color=lvgl.Palette.YELLOW.darken(4),
    )

    # Create an object with the base style only
    obj_base = lvgl.Object()
    obj_base.add_style(style_base)
    obj_base.align_as(lvgl.ALIGN_LEFT_MID, 20, 0)

    lvgl.Label(obj_base, text="Base", align=lvgl.ALIGN_CENTER)

    # Create another object with the base style and earnings style too
    obj_warning = lvgl.Object()
    obj_warning.add_style(style_base)
    obj_warning.add_style(style_warning)
    obj_warning.align_as(lvgl.ALIGN_RIGHT_MID, -20, 0)

    lvgl.Label(obj_warning, text="Warning", align=lvgl.ALIGN_CENTER)


# Local styles
def style_12():
    style = lvgl.Style(
        bg_color=lvgl.Palette.GREEN.main(),
        border_color=lvgl.Palette.GREEN.lighten(3),
        border_width=3,
    )

    obj = lvgl.Object()
    obj.add_style(style)

    # Overwrite the background color locally
    obj.bg_color = lvgl.Palette.ORANGE.main()

    obj.align = lvgl.ALIGN_CENTER


# Add styles to parts and states
def style_13():
    style_indic = lvgl.Style(
        bg_color=lvgl.Palette.RED.lighten(3),
        bg_grad_color=lvgl.Palette.RED.main(),
        bg_grad_dir=lvgl.GRAD_DIR_HOR,
    )

    style_indic_pr = lvgl.Style(
        shadow_color=lvgl.Palette.RED.main(),
        shadow_width=10,
        shadow_spread=3,
    )

    # Create an object with the new style_pr
    obj = lvgl.Slider()
    obj.add_style(style_indic, lvgl.PART_INDICATOR)
    obj.add_style(style_indic_pr, lvgl.PART_INDICATOR | lvgl.STATE_PRESSED)
    obj.value = 70
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
