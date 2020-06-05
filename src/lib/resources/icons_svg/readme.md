# UI guideline

## Tool buttons

### Preparing the svg document

* Check in the UI the placement of the icon to get its size. 
* Check also for any icons displayed next to the new one: you can start the new icon by copy-pasting one of these neighboring icon to have the right size.
* Work if possible in Inkscape, the existing icon svg were all made with Inkscape, and all the meta data for Inkscape helping the svg creation are contained in the svg file
* If there is no similar icon you can copy-paste, create a new svg document with the correct size from the beginning: if the icon is meant to be used in 24px, the svg document should be in 24x24px
* Always display the grid, and try to place points following the grid

### Designing the icon
* Make sure that the design reflect the usage, and make it simple
* Ask for advice on Gitter if any doubt
* Ensure that the design does not have too many similarities with existing icons to avoid any confusion

### Creating the svg icon
* The icon drawing should be centered
* The icon drawing should have a margin of 4px all around
![example](templates/template_on.svg)
* Not necessarily but *very* recommended rule: try to make the icon pixel perfect as possible. This does not apply to all icons but for the ones with straight lines, and with overall complicated design, the pixel-perfectness helps the design to be more clean. For the icons of 24x24px or more, it is recommended but not mandatory, but for icons in 16x16 it has to be pixel perfect or almost in order to achieve a clean and crips design. 
* Line width should be of 2 to 3px, there is no strict rules: it depends on the overall look and balance
* Line cap: square
* Line join: miter
* The design has to be declined into four states: on, off, hover and disabled. Each of them has a different name and color theme:
  * On state
    * Name: __{button name}_on.svg__  
    * Color: __#f6a019ff__ (orange)
  * Off state
    * Name: __{button name}_off.svg__  
    * Color: __#f0f0f0ff__ (light grey)
  * Hover state
    * Name: __{button name}_hover.svg__  
    * Background: square filled with __#f6a019ff__ (orange) and with a contour of __1px__ in __#000000ff__ (black)
    * Color: __#000000ff__ (black)
  * Disabled state
    * Name: __{button name}_disabled.svg__  
    * Color: __#808080ff__ (dark grey)

### Exporting the svg to png

You can use the script in __src/lib/resources/icons_svg/export_svg.sh__ with the following arguments:
* Source directory containing the svg to export (e.g. __src/lib/resources/icons_svg/toolbutton_24__)
* Destination directory for the png, it should always be __src/lib/resources/icons__
* Size in pixel for the exported png (e.g. __24__), the script will automatically create the retina version by doubling this value
* (Optional) The height in pixel, for icons with width different to height, you can add this argument and the third value for size will be interpreted as a width value


### Final check in the UI
Once the icons were exported in png and that there were integrated in the UI, check for:
* The size, it can happen the icon is not displayed in the intended size
* The different states (on, off, hover, disabled)

## Tab buttons
For tab icons (that can be found in __src/lib/resources/icons_svg/tab_icons__), there should only two states __on__ and __off__, other than that the rules for creating toolbuttons can be applied for the creation of tab icons.
