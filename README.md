# ATmega328p Serial Pulser

ATmega328p bare-metal firmware for precise pulse generation controlled via serial commands.

### 1. Wiring

- Pulse Output: PD3 (D3 in Arduino Nano) 
- Status LED: (PB5 (D13 in Arduino Nano, which is the built-in LED)

### 2. Build & Flash

Compile with
```bash
make       
```

Flash with (Requires avrdude)
```bash
make flash 
```

### 3. Operation

The generator is controlled via serial interface. Commands must be sent at 9600 baud:
```bash
stty -F /dev/ttyUSB0 9600
```

#### Modes of operation

There are three 

 - Single Pulse (0)
 - Multi-pulse burst (1)
 - Continuous (2)

You can set one of these modes by sending an `m` command. For instance, to set the generator
to continuous mode run
```bash
echo "m2\n" > /dev/ttyUSB0 
```

#### Commands 

| Command      | Description           | Parameter range | 
|--------------|-----------------------|-----------------|
| `t`          | Trigger               | -               |
| `m`          | Change operation mode | 1 (Single pulse), 2 (Multi-pulse burst), 3 (Continuous) |
| `d<param>`   | Pulse width (ms)      | 1 - 274873713   |
| `p<param>`   | Total period (ms)     | `d` - 274873713 |
| `c<param>`   | Pulse Count (mode 1)  | 1 - 255         |
| `s`          | Stop                  | -               |


#### Pulse duration and period

This uses Timer1 to count both pulse duration and pulse period. The timer can count up to $2^{16} - 1 = 65535$ 
ticks, with each tick lasting $1024/16\ \text{MHz} = 64\ \mu\text{s}$. This gives a maximum single-segment 
duration of $65535 \times 64\ \mu\text{s} = 4.19424\ \text{s}$. For longer pulse duration/periods, we chain 
multiple overflows. The total time becomes $(N + 1) \times 4.19424\ \text{s}$, where $N$ is the maximum number of
overflows. Since we are using `uint16_t` to store the required overflows, the theoretical maximum period is 
$274873713\ \text{ms} \approx 76.35\ \text{h}$.
