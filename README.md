# Wireless Gamepad using LED Matrix and nRF52840 Development Kit

## Overview
This project aims to implement a **wireless gamepad** using an **LED Matrix** and the **nRF52840 development kit**. Initially conceived as a simple game controller, we enhanced the design to incorporate additional features that offer flexibility and scalability for further expansion. Our gamepad allows users to control multiple games wirelessly and is designed with a user-friendly interface.

### Features
- **Wireless Communication**: Utilizes Bluetooth connectivity to interact with games wirelessly.
- **Game Control**: Includes buttons for selecting and terminating games, as well as a joystick for in-game movement.
- **LED Indicators**: 
  - **LED 1**: Displays joystick actions.
  - **LED 2**: Indicates Bluetooth connection status.
  - **LED Matrix**: Visualizes the direction of movement during gameplay.

## Project Components

### Hardware
- **nRF52840 Development Kit**: Used as the core controller for the gamepad. It communicates wirelessly with the server (laptop).
- **LED Matrix**: Displays direction of joystick movements.
- **Joystick**: Controls movement of objects within the game.
- **Buttons**: 
  - **Button 1**: Starts Game 1 (Snake).
  - **Button 2**: Starts Game 2 (Blocker).
  - **Button 3**: Starts Game 3 (Ping Pong).
  - **Button 4**: Terminates the current game.

### Software Architecture
We implemented the gamepad using a **Server-Client architecture**:
- **Server (Laptop)**: Acts as the central controller and runs the games.
- **Client (nRF52840 Development Kit)**: Functions as the peripheral device, transmitting user inputs from the gamepad to the server wirelessly.

### Games
1. **Game 1 - Snake**: Classic snake game where the player controls the direction of the snake.
2. **Game 2 - Blocker**: A reaction-based game that challenges the player to avoid or block obstacles.
3. **Game 3 - Ping Pong**: A digital version of the table tennis game, controlled by the joystick.

## Advantages
- **Wireless Control**: No need for wired connections, enabling flexibility and ease of use.
- **Scalability**: Designed with modularity in mind, allowing for future expansion and additional game support.
- **User-Friendly Interface**: Easy-to-use controls and intuitive LED indicators provide an enhanced user experience.
  
## Future Enhancements
This project opens up multiple avenues for expansion:
- **Multiplayer Games**: Adding support for multiple gamepads to connect to the server for multiplayer gaming.
- **Broad Device Compatibility**: Extending Bluetooth connectivity to other devices for diverse applications.
- **Enhanced Joystick Control**: Improving the control algorithm to offer more sophisticated input handling for advanced games.

## How to Run the Project
### Server Setup
1. Clone the repository on your laptop.
2. Install necessary dependencies for the game applications (Snake, Blocker, Ping Pong).
3. Run the server application to start the games.

### Client Setup
1. Flash the firmware onto the nRF52840 development kit.
2. Ensure the Bluetooth connection between the gamepad and laptop is established.
3. Use the buttons and joystick to navigate and control the games.
