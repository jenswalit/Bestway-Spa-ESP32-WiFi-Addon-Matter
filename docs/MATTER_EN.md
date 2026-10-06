# Matter pairing

**English** | [Nederlands](MATTER_NL.md)

The module uses Matter over **WiFi**. Thread is not required for this implementation. A new module creates its own setup credentials during its first initialization and stores them. OTA updates are intended to retain both these credentials and existing fabrics.

## First pairing

1. Connect the module to your home WiFi.
2. Open its web interface and the Matter settings.
3. Enable Matter if it is disabled and check that its runtime is active.
4. Use your module's permanent manual pairing code or QR code when adding a Matter device in your controller's app.

Do not publish your own pairing code. A code shown in an example screenshot belongs to that example, not to your module.

See the [English user manual](PtW_Spa_Control_User_Manual_EN.pdf) for phone and hub requirements.

## Adding another ecosystem

1. Keep the existing Matter pairing intact.
2. In the module's Matter page, select **Nieuwe koppeling openen** (Open new pairing).
3. Use the temporary pairing code or QR code shown for that commissioning window in the new ecosystem's app.
4. Complete pairing before the window expires. Open another window if necessary.

Existing Google Home, Apple Home or Home Assistant fabrics remain in place. Do not erase fabrics to add a second controller. The permanent device code and a temporary multi-admin code have different purposes.

## Understanding fabrics

The fabric count represents Matter network memberships. When pairing through iOS, a platform fabric may be created in addition to a Google fabric. This alone does not mean that the spa is already visible in the Apple Home app.

## Updates and reset

Normal logging must not publish setup codes. A deliberate Matter reset is a separate user action; it is not part of a normal update or the procedure for adding another ecosystem.

Home Assistant can use MQTT while Matter remains enabled. See [MQTT setup](MQTT_EN.md) for the MQTT connection.
