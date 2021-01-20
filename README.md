# LoRa Calculator 
**A simple command line tool for lora packet calculation.**

So, finally I am tired of the Semtech GUI tool and the customized Excel shits, let's move to the elegant cmdline tool together . 

## Feature

All below parameters are configurable:

- SF5 - SF12 spread factor
- Long Interleaving
- Low Data Rate Optimize
- Header
- CRC
- Prebuilt binary for Windows platform

## Compile

For windows platform, please install mingw first. A prebuilt binary is included in the repo. 

```bash
$ gcc lrc.c -o lrc
```

## Usage

```
./lrc -h
Usage: lrc [OPTIONS]
 -h, --help                                      Help
 -v, --version                                   Version 0.0.1
 -b, --bw,   --bandwidth                 <uint>  Bandwidth in hz, eg: 125000, 250000, 203125
 -s, --sf,   --spread-factor             <uint>  Spread factor (5 - 12)
 -o, --ldro, --low-datarate-optimize     <bool>  Low datarate optimize (on / off)
 -f, --fs,   --fine-synch                <bool>  Fine synch for high speed lora SF5 and SF6,
 -i, --li,   --long-interleaving         <bool>  Available in 2.4GHz lora (sx128x)
 -c, --cr,   --coderate                  <uint>  coding rate CR4/5 CR4/6... (1 - 4)
 -r, --pr,   --preamble                  <uint>  Raw preamble lenght
 -d, --hdr,  --header                    <bool>  Header option (on / off)
 -k, --crc,                              <bool>  CRC option (on / off)
 -l, --pl,   --payload                   <uint>  payload length
```

## Example

### Default Paramters

```
./lrc

sf 7, bw 125000, cr 1, ldro off, fs off, li off, pr 8, hdr on, crc on, pl 12
toa: 41216us, 41.216ms, 0.041s
```

![image-20210120163914135](https://img.juzuq.com/20210120-163918-123.png)

## SF12 with Low Data Rate Optimize On

```
./lrc --sf=12

toa: 1155072us, 1155.072ms, 1.155s
sf 12, bw 125000, cr 1, ldro on, fs off, li off, pr 8, hdr on, crc on, pl 12
```

![image-20210120164441045](https://img.juzuq.com/20210120-164443-628.png)

### SF12 with Low Data Rate Optimize Off

```
./lrc --sf=12 --ldro=off

toa: 991232us, 991.232ms, 0.991s
sf 12, bw 125000, cr 1, ldro off, fs off, li off, pr 8, hdr on, crc on, pl 12
```

![image-20210120164513969](https://img.juzuq.com/20210120-164612-732.png)

## Acknowledgement 

- Original calculator API https://github.com/Lora-net/gateway_2g4_hal/blob/master/libloragw/src/loragw_hal.c#L492
- Some code are copied from https://github.com/JiapengLi/lorawan-parser/blob/master/util/parser/main.c