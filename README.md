# blt-sound-daemon

blt-sound-daemon is a user-level service for controlling the sound volume of Bluetooth devices, specifically designed for Focusrite audio interfaces. It acts as an interface between the Bluetooth stack (BlueZ) and PulseAudio, allowing seamless control of volume levels.

## Purpose

The purpose of blt-sound-daemon is to provide a convenient solution for managing Bluetooth audio volume on Focusrite devices. It monitors the volume changes of Bluetooth devices and adjusts the system volume accordingly using PulseAudio, ensuring consistent volume control across different audio sources.

## Use Cases

- **Focusrite Bluetooth Integration**: blt-sound-daemon is tailored specifically for Focusrite audio interfaces with Bluetooth capabilities. It enhances the user experience by synchronizing the Bluetooth audio volume with the system volume, eliminating the need to adjust volume separately for each device.
- **Seamless Bluetooth Audio Control**: By running blt-sound-daemon as a user service, users can enjoy the benefits of automatic volume adjustment without requiring superuser privileges or system-level configurations.
- **Customizability**: blt-sound-daemon can be easily configured to suit specific preferences or adapt to different Bluetooth audio devices by modifying the source code or adjusting the service settings.

## Usage

To use blt-sound-daemon, follow these steps:

1. Clone the repository or download the source code.
2. Configure the necessary dependencies (refer to source for details).
3. Build and install the blt-sound-daemon binary.
4. Configure blt-sound-daemon as a user service, typically under ~/.config/systemd/user/ (not system service).
5. Start the blt-sound-daemon service.
6. Enjoy seamless Bluetooth audio volume control on your Focusrite audio interface.

## License

This project is licensed under the MIT License.

Feel free to modify and adapt blt-sound-daemon to suit your specific needs. Contributions and feedback are welcome.
