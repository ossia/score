# Using the Factor Oracle process in score

This process allows to improvise upon a chain of symbols.
The symbols currently supported are the alphabetic characters.

## Ports

### Input

* `in`: Receives external symbols.
* `regen`: Send an impulse here to regenerate a new chain upon which improvisation happens.
* `bang`:  Send an impulse here to get the next symbol in the generated chain.

### Output

* `out`: Will output a symbol when an impulse is received in the `bang` input.

## Usage

The process works as follows:
* Beforehand, define the sequence length expected for the oracle.
* Send elements one by one in the first port.
* When there have been enough elements in the input to produce meaningful output, send an impulse to  `regen`.
  This will use the input data to generate a new chain upon which improvisation can happen.
* At this point, it is possible to send bangs to get improvised output which follow the input.

