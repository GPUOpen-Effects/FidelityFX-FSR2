
2022-09-13 | FidelityFX Super Resolution 2.1.1
-------
- Fix issue with reprojection data on a reset.

2022-09-06 | FidelityFX Super Resolution 2.1
-------
- Reactivity mask now uses full range of value in the mask (0.0 - 1.0).
- Reactivity and Composition and Transparency mask dialation is now based on input colors to avoid expanding reactiveness into non-relevant upscaled areas.
- Disocclusion logic improved in order to detect disocclusions in areas with very small depth separation.
- RCAS Pass forced to fp32 mode to reduce chance of issues seen with HDR input values.
- Fix for display-resolution motion vectors interpretation.
- FP16/FP32 computation review, readjusting balance of fp16/fp32 for maximum quality.
- Amended motion vector description within the documentation.
- Various documentation edits for spelling.
- Clarified the frame delta time input value within the readme documentation.
- Fixed issue with bad memset within the shader blob selection logic.


2022-06-22 | FidelityFX Super Resolution 2.0.1
-------
- First release.

