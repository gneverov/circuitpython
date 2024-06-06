import lvgl


# Basic example to create a "Hello world" label
def get_started_1():
    # Change the active screen's background color
    lvgl.display.screen.bg_color = 0x003A57

    # Create a white label, set its text and align it to the center
    lvgl.Label(text="Hello world", text_color=0xFFFFFF, align=lvgl.ALIGN_CENTER)


# Create a button with a label and react on click event.
def get_started_2():
    def btn_event_cb(e):
        if e.code == lvgl.EVENT_CLICKED:
            btn.count += 1
            lbl.text = f"Button: {btn.count}"

    btn = lvgl.Button(  # Add a button the current screen
        x=10,  # Set its position
        y=10,
        width=120,  # Set its size
        height=50,
    )
    btn.count = 0
    btn.add_event_cb(btn_event_cb, lvgl.EVENT_ALL)  # Assign a callback to the button

    lbl = lvgl.Label(
        btn,  # Add a label to the button
        text="Button",  # Set the labels text
        align=lvgl.ALIGN_CENTER,
    )


# Create styles from scratch for buttons.
def get_started_3():
    # Create a simple button style
    style_btn = lvgl.Style(
        radius=10,
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.GREY.lighten(3),
        bg_grad_color=lvgl.Palette.GREY.main(),
        bg_grad_dir=lvgl.GRAD_DIR_VER,
        border_color=lvgl.color_black(),
        border_opa=lvgl.OPA_20,
        border_width=2,
        text_color=lvgl.color_black(),
    )

    # Create a style for the pressed state.
    # Use a color filter to simply modify all colors in this state
    style_button_pressed = lvgl.Style(
        color_filter_dsc=lvgl.ColorFilter.DARKEN,
        color_filter_opa=lvgl.OPA_20,
    )

    # Create a red style. Change only some colors.
    style_button_red = lvgl.Style(
        bg_color=lvgl.Palette.RED.main(),
        bg_grad_color=lvgl.Palette.RED.lighten(3),
    )

    # Create a button and use the new styles
    btn = lvgl.Button(x=10, y=10, width=120, height=50)
    # Remove the styles coming from the theme
    # Note that size and position are also stored as style properties
    # so lv_obj_remove_style_all will remove the set size and position too
    # lv_obj_remove_style_all(btn);
    btn.add_style(style_btn)
    btn.add_style(style_button_pressed, lvgl.STATE_PRESSED)

    # Add a label to the button
    lvgl.Label(btn, text="Button", align=lvgl.ALIGN_CENTER)

    # Create another button and use the red style too
    btn2 = lvgl.Button(x=10, y=80, width=120, height=50)
    btn2.add_style(style_btn)
    btn2.add_style(style_button_red)
    btn2.add_style(style_button_pressed, lvgl.STATE_PRESSED)
    btn2.radius = lvgl.RADIUS_CIRCLE

    lvgl.Label(btn2, text="Button 2", align=lvgl.ALIGN_CENTER)


# Create a slider and write its value on a label
def get_started_4():
    def slider_event_cb(e):
        # Refresh the text
        label.text = str(slider.value)
        label.align_to(slider, lvgl.ALIGN_OUT_TOP_MID, 0, -15)  # Align top of the slider

    # Create a slider in the center of the display
    slider = lvgl.Slider(
        width=200,  # Set the width
        align=lvgl.ALIGN_CENTER,  # Align to the center of the parent (screen)
    )
    slider.add_event_cb(slider_event_cb, lvgl.EVENT_VALUE_CHANGED)  # Assign an event function

    # Create a label above the slider
    label = lvgl.Label(text="0")
    label.align_to(slider, lvgl.ALIGN_OUT_TOP_MID, 0, -15)  # Align top of the slider
