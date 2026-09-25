# Generates the fallback fixture of multi-outlet.sh, used when the Depth
# Anything 3 model and its photo are not on the machine: a 2-output image
# model with the same I/O as DA3-metric (1x3x280x504 -> depth 1x1x280x504 in
# metres, sky 1x1x280x504 in [0,1]) and a scene with a sky and some objects.
import os
import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper
from PIL import Image, ImageDraw

here = os.path.dirname(os.path.abspath(__file__))

# depth = 3 + 2 * red, sky = sigmoid(3 * (blue - red)), on the ImageNet-
# normalised input, so each output has its own structure.
nodes = [
    helper.make_node("Slice", ["image", "r0", "r1", "axis"], ["red"]),
    helper.make_node("Slice", ["image", "b0", "b1", "axis"], ["blue"]),
    helper.make_node("Mul", ["red", "two"], ["red2"]),
    helper.make_node("Add", ["red2", "three"], ["depth"]),
    helper.make_node("Sub", ["blue", "red"], ["bmr"]),
    helper.make_node("Mul", ["bmr", "three"], ["bmr3"]),
    helper.make_node("Sigmoid", ["bmr3"], ["sky"]),
]
inits = [
    numpy_helper.from_array(np.array([0], np.int64), "r0"),
    numpy_helper.from_array(np.array([1], np.int64), "r1"),
    numpy_helper.from_array(np.array([2], np.int64), "b0"),
    numpy_helper.from_array(np.array([3], np.int64), "b1"),
    numpy_helper.from_array(np.array([1], np.int64), "axis"),
    numpy_helper.from_array(np.array(2.0, np.float32), "two"),
    numpy_helper.from_array(np.array(3.0, np.float32), "three"),
]
g = helper.make_graph(
    nodes, "two_outputs",
    [helper.make_tensor_value_info("image", TensorProto.FLOAT, [1, 3, 280, 504])],
    [helper.make_tensor_value_info("depth", TensorProto.FLOAT, [1, 1, 280, 504]),
     helper.make_tensor_value_info("sky", TensorProto.FLOAT, [1, 1, 280, 504])],
    inits)
m = helper.make_model(g, opset_imports=[helper.make_opsetid("", 13)])
m.ir_version = 8
onnx.checker.check_model(m)
onnx.save(m, os.path.join(here, "two_outputs.onnx"))

# A sky gradient over red / brown / grey objects.
w, h = 640, 480
img = Image.new("RGB", (w, h))
d = ImageDraw.Draw(img)
for y in range(h // 2):
    t = y / (h / 2)
    d.line([(0, y), (w, y)], fill=(int(90 + 60 * t), int(150 + 50 * t), 235))
d.rectangle([0, h // 2, w, h], fill=(110, 95, 70))
d.ellipse([60, 250, 260, 450], fill=(230, 40, 30))
d.rectangle([330, 200, 560, 420], fill=(150, 150, 150))
d.polygon([(420, 90), (520, 230), (320, 230)], fill=(200, 120, 60))
img.save(os.path.join(here, "scene.png"), optimize=True)
