
 12:55 PM 2/09/2026 First Road block:

Deciding on how to handle braking was a real challenge, so i had to come up with a solution

    First the requirement , brake must never be dropped and not wait behind another task

    I kept a very simple solution to this , now if queue is full , evict the oldest command not consumed and then queue in the break
    This satisfies a break never being dropped (kind off) and override any other throttle command.
    The only trade off is that a brake could be dropped for another brake.

    Another thing that needed to be handled , is out of bound values for commands with numerical inputs , they are discarded if out of bound.

    Commandrx logs the latest command into the queue and changes a shared variable indicating the last command addmitted into the queue and the latest time.

    Actuate task does three main things , first it consumes the queue and updates state variables
    the steer value
    the throttle value
    prints to the uart if command was ping
    and also sets the braking state

    the second thing it does is actually assert the led ,
    if link is lost ( variable updated by watchdog )
    blinks constantly the LED
    If in a braking state,  drives led to 0
    otherwie ramps led pwm value to throttle value from previous value

    It then blocks till queue is full or times out and gives up the CPU

    Advantage to this is there is only a single task that controls the LED
    and the only thing that the watch dog needs to is check if the link is lost


 11:36 PM 3/09/2026

     Second major conflict , if i send two commands in one line , it does only parses the first and discards the second or any following commands ,
     this may conflict with the final test case,
     I will assume that 2 commands could be sent on one line and nothing more.
     This should help pass the test case and make the parser easy enough to implement

     Another minor detail Actuator_SetOutput(uint8_t percent) , ramps up the output and does not set to the target immediately
     This matters on a real motor to prevent current spikes
     the actuator task calls it every 5 ms
     So now actuate task calls two functions depending if the link is lost or not
     if the link is lost it blinks twice a second with half brightness (to distinguish between it and motor activity)











Timing reqs

COMMAND_RX — waiting for serial bytes, Wakes on a
ACTUATE — blocks on the queue (your xQueuePeek with timeout). Wakes when a command arrives or the timeout fires.
No vTaskDelay needed; the queue wait is its block.
WATCHDOG — periodic. vTaskDelay (or vTaskDelayUntil) of ~50–100 ms. It only needs to check the timestamp a few times per second.
STATUS — periodic, once a second. vTaskDelayUntil(1000ms).
