# Deferred Logging Proof of Concept

> [!NOTE]
> This proof of concept was written to illustrate [this article](https://blog.gistre.epita.fr/posts/sofiane.meftah-2025-06-15-deferred_logging/)

If there is at most N distinct log messages, then log2(N) bits is enough to uniquely identify each one. Thus, instead of transmitting the format string every time, we can simply send a log unique ID alongside the format parameters. The simplest form of unique ID, in software, is a pointer, as only a single string/instruction can be stored at a specific address.

> Note that pointers are not memory efficient unique IDs as a lot of possible pointers are not valid, for example, pointing in the middle of an instruction or a string.
>
> For example, consider a 1KB section containing 34 strings, a pointer must be at least 10 bits to address any byte in the section. However, 34 strings can be uniquely addressed by 6 bits (2^6 = 64 possible values).

The idea is very simple : instead of sending the string contents, simply send the string’s pointer as the log unique ID. The host, receiving the logs, will build a map of pointers to log messages once and formats the incoming logs on the fly.

Advantages:
-  Lower CPU time when logging as most of the CPU time is spent formatting the string.
-  Lower power consumption as no formatting is done regardless of the presence of an observer.
-  Small code size as we no longer need to embed the formatting logic.
-  Lower IO bandwidth as only the few neceassary bytes are transmitted.
