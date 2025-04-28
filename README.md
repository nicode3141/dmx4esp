# DMX4ESP - DMX Library for Esp32
This library helps sending and receiving DMX-512 data using any esp32.

## About DMX-512

DMX is a protocol primarily used in the lighting industry, based on the RS485 standard.
It is an acronym for **D**igital **M**ultiplex

## The DMX-512 Protocoll

DMX 512


| ------------- | ------------- | ------------- | ------------- |------------- |
| Parameter | 1. BREAK | 2. Mark After Break | 3. Start Code (0x00) | gh |


| Parameter | 1. BREAK | 2. Mark After Break | 3. Start Code (0x00) | 4. DMX Channel Data (1- 512) | 
| ------------- | ------------- | ------------- | ------------- |------------- |
| **Timing** | >88µs | >12µs | - | - | 

this integration currently supports following chipsets: MAX485, 74HC04D