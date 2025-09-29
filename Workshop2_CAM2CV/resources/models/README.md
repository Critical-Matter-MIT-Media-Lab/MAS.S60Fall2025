# Model Weights Cache

Place large pre-trained model weights here when building the workshop kit offline. The Python modules automatically look in this folder before attempting any downloads.

| File | Used By | Notes |
|------|---------|-------|
| `emotion-ferplus-8.onnx` | `cv-modules/facial_expression_recognition.py` | FER+ emotion classifier (8 classes) |
| `yolov8n.pt` | `cv-modules/object_detection.py` | Ultralytics YOLOv8 nano model |

> Keep filenames exact. The scripts print detailed instructions if a file is missing.
