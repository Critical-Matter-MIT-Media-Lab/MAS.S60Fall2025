# Facilitator Checklist

## One Week Before
- [ ] Verify all hardware boots (ESP32S3 + camera ribbon cable seated correctly).
- [ ] Clone the repository and run through each Python module end-to-end.
- [ ] Download large model files (`emotion-ferplus-8.onnx`, `yolov8n.pt`) into `resources/models/` for offline backup.
- [ ] Prepare a hotspot SSID/password card for participants.
- [ ] Confirm venue Wi-Fi policies (firewall rules for ESP32 devices, open ports).

## Day Of (Setup)
- [ ] Flash the firmware with workshop-specific Wi-Fi credentials.
- [ ] Power the board and confirm `http://<device-ip>/` loads on a demo laptop.
- [ ] Start the Python virtual environment; pre-download dependencies to avoid locking bandwidth later.
- [ ] Launch the p5.js Live Server page and ensure the stream renders.
- [ ] Arrange HDMI capture/recording if projecting the demo.

## During the Session
- [ ] Share the Git repo URL or zipped package with attendees.
- [ ] Encourage students to work in pairs (one handles firmware, the other the CV scripts).
- [ ] Capture screenshots/recordings for recap emails.
- [ ] Keep `docs/troubleshooting.md` open for quick reference.

## After The Session
- [ ] Collect participant feedback and note tricky sections.
- [ ] Commit any improvements or bug fixes discovered during the workshop.
- [ ] Recharge batteries/power banks and safely pack all hardware.
