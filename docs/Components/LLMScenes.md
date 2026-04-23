# LLMScenes

**Role:** Creative scene generation using the Google GenAI API and DSL.

## Key Facts
- **SDK:** Uses the new `google-genai` library (migrated from `google-generativeai`).
- **Model:** Uses `gemini-2.0-flash` (with `1.5-flash` fallback).
- **Process:**
    1. Sends a `SYSTEM_PROMPT` explaining the matrix constraints and DSL syntax.
    2. Receives a DSL script based on a user-provided theme (e.g., "Cyberpunk").
    3. Executes the script immediately via `DSLRunner`.
- **Security:** Requires `LLM_API_KEY` in `.env.secrets`.

## CLI Usage
```bash
python llm_scenes.py "A tropical beach at sunset"
```
