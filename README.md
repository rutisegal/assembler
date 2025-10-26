---

# 📁 Assembler Project – README

This project was developed as part of **course 20465**.
It implements a **two-pass assembler** in **ANSI C**, capable of translating assembly source files into 16-bit machine code suitable for **loading and linkage**.

---

## 🛠️ Project Requirements and Compilation

### Compilation

The project must be compiled using the **gcc** compiler with the following strict flags:

```bash
gcc -Wall -ansi -pedantic
```

The code must compile **without any warnings** under these flags.

### Input Format

The program accepts a list of assembly source file names (without the `.as` extension) as command-line arguments.
For example:

```bash
./assembler prog1 prog2
```

will process `prog1.as` and `prog2.as`.

---

## 📝 Supported Assembly Language Features

### Instructions and Registers

1. **Instruction Length:** All machine instructions and data words are 16 bits long.
2. **Registers:** Supports **r0 – r7**, represented using **3 bits**.
3. **Addressing Modes:** Four addressing methods (encoded using 2 bits):

   * 0 – Immediate
   * 1 – Direct
   * 2 – Index
   * 3 – Register Direct

### Directives

* `.data`, `.string`, `.mat` — Define data sections.
* **Labels:** Mark memory locations (followed by a colon `:`).
* **Entry/External:**

  * `.entry` — Marks a symbol as an entry point.
  * `.extern` — Declares a symbol as external (defined in another file).

### Macros

The assembler supports macro definitions using `.mcro` and `.mcroend`.
Macro expansion occurs in an initial pre-processing phase before the main two passes.

---

## ⚙️ Assembly Process and Memory Mapping

The assembler operates in **two passes**:

1. **First Pass:**

   * Builds the **Symbol Table** (labels, attributes, and addresses).
   * Calculates instruction and data addresses (IC/DC).

2. **Second Pass:**

   * Resolves symbol references and generates the final machine code.

### Memory Management

* **Starting Address:** Memory begins at address **100**.
* **Symbol Table:** Stores label names, memory addresses, and attributes (code/data/external).

### Output Files

If assembly completes successfully, the following files are produced for each input file:

| File   | Description                           |
| ------ | ------------------------------------- |
| `.ob`  | Object file – translated machine code |
| `.ext` | List of external symbols used         |
| `.ent` | List of entry symbols                 |

Example:

```
prog.as  →  prog.ob, prog.ext, prog.ent
```

If any errors are found, they are printed to **stdout**, and **no output files** are generated.

---

## 🚀 Encoding Mechanism – From Assembly to 16-bit Machine Code

### A, R, E Fields (Bits 0–1)

The two least significant bits define the reference type:

| Bits (0–1) | Type            | Meaning                                                  |
| :--------: | :-------------- | :------------------------------------------------------- |
|   **00**   | Absolute (A)    | Fixed address reference                                  |
|   **01**   | Relocatable (R) | Refers to an internal label (may be adjusted on linkage) |
|   **10**   | External (E)    | Refers to an externally defined symbol                   |

---

### Instruction Word Format

| Bit Range | Length | Field       | Description                              |
| :-------: | :----- | :---------- | :--------------------------------------- |
|    0–1    | 2      | A, R, E     | Addressing reference bits                |
|    2–3    | 2      | Destination | Addressing method of destination operand |
|    4–5    | 2      | Source      | Addressing method of source operand      |
|    6–9    | 4      | Opcode      | Operation code                           |
|   10–15   | 6      | Misc        | Additional instruction info              |

Specific instructions such as **`bne`** (branch if not equal) use the **Z flag** in the **Program Status Word (PSW)** to control branching behavior.

---

### Register Encoding in Combined Words

When both operands are registers, they are encoded in a single 16-bit word following the main instruction:

* Bits **6–9:** Source Register
* Bits **2–5:** Destination Register
* Bits **0–1:** `00` (Absolute)

---

### Data Encoding

1. **`.data` / `.mat`** — Numeric constants stored as 16-bit words.
2. **`.string`** — Characters stored sequentially (one per 16-bit word), terminated by a **null character (`\0`)** encoded as `0`.

---

## 📚 Notes

* Fully implemented in **ANSI C**.
* Follows the official **20465 assembler specification** and strict compilation rules.
* Designed for modularity, clarity, and accurate two-pass processing.

---
