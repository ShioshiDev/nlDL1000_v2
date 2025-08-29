---
applyTo: "**"
---

# AI Agent Instructions

## Archon Integration & Workflow
**CRITICAL:** This project uses Archon for knowledge management, task tracking, and project organization.

**MANDATORY:** Always complete the full Archon task cycle before any coding:
1. Check Current Task → Review task details and requirements
2. Research for Task → Search relevant documentation and examples
3. Implement the Task → Write code based on research
4. Update Task Status → Move task from "todo" → "doing" → "review"
5. Get Next Task → Check for next priority task
6. Repeat Cycle

**Task Management Rules:**
- Update all actions to Archon
- Move tasks from "todo" → "doing" → "review" (not directly to complete)
- Maintain task descriptions and add implementation notes
- DO NOT MAKE ASSUMPTIONS - check project documentation for questions
- DO NOT SKIP STEPS - follow the full task cycle

---

# Project Structure
The project follows a modular structure with the following key components:
app (esp32 boot code and main application loop)
- coreManager (the core orchestration engine of the application)
- networkingManager (handles networking initialization and communication and event handling and status updates)
- connectivityManager (manages connectivity status)
- servicesManager (handles background services like the mqtt clients)
- displayManager (manages display output based on the status view model)
- ledManager (controls LED indicators based on the status view model)
- statusViewModel (provides a central status information structure to be used by other components for status updates)

# Additional Notes
- Ensure all components are properly integrated and communicate with each other.
- Follow best practices for modular design and code organization.
- The project is a C++ application for an ESP32 Arduino project in PlatformIO. The code should be written with performance and efficiency in mind.
- Use non-blocking code wherever possible to ensure smooth operation.
- When using PlatformIO, use the build task to compile code so that the agent can obtain the output from the compilation process. !**Wait until the build process completes with  "*  Terminal will be reused by tasks, press any key to close it." before determining success or if there are any errors. DO NOT FINISH WATCHING UNTIL THE PROCESS IS COMPLETE**!