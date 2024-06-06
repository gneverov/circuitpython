import lvgl


def button_1():
    def event_handler(e):
        if e.code == lvgl.EVENT_CLICKED:
            print("Clicked")
        elif e.code == lvgl.EVENT_VALUE_CHANGED:
            print("Toggled")

    btn1 = lvgl.Button(
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=-40,
    )
    btn1.add_event_cb(event_handler, lvgl.EVENT_ALL)
    btn1.flags &= ~lvgl.OBJ_FLAG_PRESS_LOCK

    lvgl.Label(
        btn1,
        text="Button",
        align=lvgl.ALIGN_CENTER,
    )

    btn2 = lvgl.Button(
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=40,
        height=lvgl.SIZE_CONTENT,
    )
    btn2.add_event_cb(event_handler, lvgl.EVENT_ALL)
    btn2.flags |= lvgl.OBJ_FLAG_CHECKABLE

    lvgl.Label(
        btn2,
        text="Toggle",
        align=lvgl.ALIGN_CENTER,
    )


# Style a button from scratch
def button_2():
    # Init the style for the default state
    style = lvgl.Style(
        radius=3,
        bg_opa=lvgl.OPA_100,
        bg_color=lvgl.Palette.BLUE.main(),
        bg_grad_color=lvgl.Palette.BLUE.darken(2),
        bg_grad_dir=lvgl.GRAD_DIR_VER,
        border_opa=lvgl.OPA_40,
        border_width=2,
        border_color=lvgl.Palette.GREY.main(),
        shadow_width=8,
        shadow_color=lvgl.Palette.GREY.main(),
        shadow_offset_y=8,
        outline_opa=lvgl.OPA_COVER,
        outline_color=lvgl.Palette.BLUE.main(),
        text_color=lvgl.color_white(),
        pad_left=10,
        pad_right=10,
        pad_top=10,
        pad_bottom=10,
    )

    # Init the pressed style
    style_pr = lvgl.Style(
        # Add a large outline when pressed
        outline_width=30,
        outline_opa=lvgl.OPA_TRANSP,
        translate_y=5,
        shadow_offset_y=3,
        bg_color=lvgl.Palette.BLUE.darken(2),
        bg_grad_color=lvgl.Palette.BLUE.darken(4),
    )

    # Add a transition to the outline
    style_pr.transition = lvgl.StyleTransitionDsc(
        ["outline_width", "outline_opa"], lvgl.AnimPath.LINEAR, 300, 0
    )

    btn1 = lvgl.Button(
        width=lvgl.SIZE_CONTENT,
        height=lvgl.SIZE_CONTENT,
        align=lvgl.ALIGN_CENTER,
    )
    # lv_obj_remove_style_all(btn1);                          /*Remove the style coming from the theme*/
    btn1.add_style(style)
    btn1.add_style(style_pr, lvgl.STATE_PRESSED)

    lvgl.Label(
        btn1,
        text="Button",
        align=lvgl.ALIGN_CENTER,
    )


# Create a style transition on a button to act like a gum when clicked
def button_3():
    # Properties to transition
    props = ["width", "height", "text_letter_space"]

    # Transition descriptor when going back to the default state.
    # Add some delay to be sure the press transition is visible even if the press was very short
    transition_dsc_def = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.OVERSHOOT, 250, 100)

    # Transition descriptor when going to pressed state.
    # No delay, go to presses state immediately
    transition_dsc_pr = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.EASE_IN_OUT, 250, 0)

    # Add only the new transition to he default state
    style_def = lvgl.Style()
    style_def.transition = transition_dsc_def

    # Add the transition and some transformation to the presses state.
    style_pr = lvgl.Style(
        transform_width=10,
        transform_height=-10,
        text_letter_space=10,
    )
    style_pr.transition = transition_dsc_pr

    btn1 = lvgl.Button(
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=-80,
    )
    btn1.add_style(style_pr, lvgl.STATE_PRESSED)
    btn1.add_style(style_def)

    lvgl.Label(btn1, text="Gum")
