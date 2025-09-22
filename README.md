# Smartify24 - A Cloud-Based Smart Home Solution  

A comprehensive smart home system aimed at providing users with intuitive and powerful control over their home devices. The project focuses on a cost-effective, scalable solution that leverages existing web and IoT technologies.  

## Project Overview and Objectives  
The core objective of Smartify24 is to enable users to control their home appliances remotely and intelligently. By integrating a secure web platform with a custom-programmed microcontroller, we provide a seamless experience for managing smart devices. This initial phase focuses on lighting control, with a clear roadmap for future expansion to other devices.  

## Minimum Viable Product (MVP)  
The MVP for Smartify24 is a functional prototype that demonstrates the core features of the system. It will include:  

- **User Authentication:** A secure login and registration system, allowing users to create an account and manage their devices.  
- **Web-Based Control Panel:** A user-friendly dashboard where authenticated users can view the status of their connected devices.  
- **Real-time Control:** The ability to instantly turn a light bulb ON or OFF with a single click from the web interface.  
- **Timer and Scheduling:** Functionality to set a duration for the light to remain ON (e.g., "keep ON for 15 minutes") or to schedule a specific time for it to turn ON/OFF (e.g., "turn ON at 7:00 PM").  
- **Device Status Check:** The ability to query the current ON/OFF status of the light bulb at any time from the dashboard.  

## Technical Architecture  
The system is divided into two primary components: the **Web Application** and the **IoT Hardware**. They communicate seamlessly to provide a reliable user experience.  

### Web Application  
The front-end of the web application is built with a modern, responsive design using **HTML, CSS, and JavaScript**. The backend, which handles user authentication, device state management, and scheduling logic, will be developed using a robust framework to ensure security and scalability. The communication between the web server and the IoT hardware will be handled via a secure API.  

### IoT Hardware  
The hardware component is built around the versatile **ESP8266 microcontroller**. This low-cost, Wi-Fi-enabled chip acts as the brain of the device, communicating with the web application over a secure connection. A **relay module** is used to safely switch the 220V power supply to the **9W light bulb**. The ESP8266 is custom-programmed to listen for commands from the server and execute them, as well as to report its current status back to the web application. This firmware is optimized for efficiency and reliability.  

---

**Designed with ☕️ by Mahmoud Sadrian**  
