# RigidLabeler Data - Labels Directory

This directory stores registration labels in JSON format.

## File Naming Convention

Labels are saved with the format:
```
{label_id}_{fixed_image_stem}_{moving_image_stem}.json
```

Where `label_id` is an 8-character hash derived from the image paths.

## Label Structure

See `docs/label_format.md` for the complete JSON schema.
