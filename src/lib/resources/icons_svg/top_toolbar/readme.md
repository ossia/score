# Top toolbar icons guideline

The global workflow to create a top toolbar button icons:
1. Duplicate the template file and rename the prefix **template** with the button name (lowercase with no spaces and underscores if composed of more than one word)
2. Follow the color theme stated below and check that the icon is centered and fitting inside the inside square as stated in the General paragraph
3. Export in the folder **/src/lib/resources/icons/** in two png files of size:
* 48 x 48 (px)
* 96 x 96 (px) with the suffix **_@2x** in the name

## Making the SVG
A template file is available for each three states (**template_{disable;off;on}.svg**)  so that any new icon can be created by duplicating the template.

### General
* Make with Inkscape if possible (to avoid weird conversion problem)
* Size: 48px x 48px
* Three different svgs per button: disabled/off/on
* The icon should be centered and inside approximately a 25 x 25 (px) square, corresponding to the layer **inside** in the template files
* If possible, convert the objects into paths (seems like there is sometimes a display error otherwise) 

### Disabled state

* Name: {*button name*}**_disabled.svg**
* Contains one layer with the icon
* Icon color: **#808080ff** (dark grey)

### Off state

* Name: {*button name*}**_off.svg**
* Contains one layer with the icon
* Icon color: **#f0f0f0ff** (light grey)

### On state

* Name: {*button name*}**_on.svg**
* Contains two layers: one for the icon and the other for the background
* The background has a contour of **1px** in **#000000ff** filled in **#f6a019ff**
* Icon color: **#f0f0f0ff** (light grey)

