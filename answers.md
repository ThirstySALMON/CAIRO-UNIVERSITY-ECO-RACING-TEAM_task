Q1. RX takes 4 
ACTUATE takes 3 
Watchdog takes 3 
and status 1 (lowest)

Reason , rx is the lifeline , anything happens due to commands received through it 
actuate commands are the one that set motors and other events in action so they take a high priority too 
watch dog also has a priority of 3 because it is short (will not compete much with actuate) and very important to notify if there is something wrong with the link
status is just a logging task

Q2.A stale steer means the car keeps going where it was , however a missing or stale brake means that the car keeps accelerating and might have a catastrophic ending 
Rather than silently pinging the link only it blinks and switches to a fail safe state.

Q3. I would add a separate printing over tx task instead of it taking time from the actuate task 
I would make a more full fledged state machine and a more defined lost link state 
add more thought into priority assigning.
