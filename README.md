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
| `d<param>`   | Pulse width (ms)      | 1 - 65535       |
| `p<param>`   | Total period (ms)     | `d` - 65535     |
| `c<param>`   | Pulse Count (mode 1)  | 1 - 255         |
| 's'          | Stop                  | -               | 
