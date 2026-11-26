# AI speed update

Both text and voice use a shared sticky round-robin pool, ranked by the mean of the two supplied tests. Models must have appeared under 10 seconds in both tests and be general text generators.

| Model | Test 1 | Test 2 | Mean (s) |
|---|---:|---:|---:|
| gemini-3.5-flash-lite | 0.68 | 0.91 | 0.795 |
| gemini-3.1-flash-lite-preview | 0.96 | 0.96 | 0.96 |
| gemini-3-flash-preview | 1.09 | 1.12 | 1.105 |
| gemini-flash-lite-latest | 1.13 | 1.95 | 1.54 |
| gemini-3.1-flash-lite | 0.92 | 7.35 | 4.135 |

10-second model attempt deadline, existing 3.5-second connect and 8-second socket read timeouts. A timed-out transport may still finish; its late response is discarded and its slot is not reused while the transport is active. HTTP 429 switches immediately to next available model; 60-second per-model cooldown, not an added wait before the next attempt. Timeout/transport failures cool down 30 seconds. Thought parts are skipped; obvious visible reasoning tags/prefixes exclude that model from this key/session. Untagged natural-language reasoning cannot be reliably identified solely from timings. No live API or Android build/device test performed.

Host tests passed: chat_bot_test, voice_ai_core_test, chat_bot_runtime_test, voice_ai_runtime_test.
