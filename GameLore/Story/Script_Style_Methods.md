# Script Style Methods

> *How dialogue and narrative are delivered in Ivory Blade.*

---

## Overview

Ivory Blade is a **VR game**. The player experiences the world in first person through the protagonist's eyes. Every delivery method must account for VR immersion -- the player is physically inside the scene, not watching it from outside.

Every line in a game script has a delivery method -- how it reaches the player. These categories define the types of scripted content in Ivory Blade and when each is used.

**Human voices are rare.** Characters communicate primarily through **climactic beeps** -- expressive, tonal sound cues that convey emotion, urgency, and personality without full voice acting. Actual spoken human voice is reserved for critical moments where the words themselves must be heard. Most dialogue is delivered as on-screen text accompanied by character-specific beep patterns, in the style of The Legend of Zelda: Ocarina of Time.

---

## The Beep System

Each character has a **unique beep signature** -- a distinct sound pattern that plays as their text appears on screen. These beeps communicate tone and emotion:

- **Pitch** conveys mood (higher = lighter/urgent, lower = serious/threatening)
- **Speed** conveys emotion (rapid = excited/panicked, slow = calm/contemplative)
- **Rhythm** conveys personality (steady = professional, irregular = anxious, sharp = aggressive)

The player reads the dialogue text while hearing the character's beep pattern. This is the primary mode of communication for most of the game. Full human voice breaks through only at the most important moments -- making those moments hit harder.

---

## Categories

### 1. CUTSCENE

In-engine cinematic sequences in the style of Ocarina of Time. The camera pulls out of first person and moves to **directed angles** -- close-ups, wide shots, dramatic framing. Characters are staged and animated. Dialogue is delivered through **text and beeps** with climactic camera work.

**Human voice is rare in cutscenes.** Most cutscene dialogue plays as text with character beeps. When a line is fully voiced, it should be marked as `[VOICED]` in the script -- this signals a moment of heightened importance.

**Camera behavior:**
- Cuts between characters during conversation
- Dramatic angles for reveals and confrontations
- Held shots on reactions -- let the beeps and text carry the weight
- Climactic beep crescendos at story peaks (the beeps intensify, accelerate, and layer)

**When used:**
- Major story reveals
- Character introductions
- Unwinnable encounters (e.g., the tiger captain's beatdown)
- Story transitions between chapters or major beats

**Format:**
```
[CUTSCENE]
SPEAKER: Dialogue line. (beeps)
SPEAKER: Dialogue line. [VOICED]
(Camera direction / stage direction.)
```

---

### 2. FACE-TO-FACE (First Person)

The player remains in first-person VR. A character is physically present and talking to the protagonist. The player may be **soft-locked** to face the speaker to a degree -- the camera gently pulls toward the character so the player is looking at them, but full head movement is not completely restricted. The player can glance around but is guided to face the person speaking.

Dialogue is delivered through **text and beeps**. The player sees the character's face, their expressions, their body language -- all in first person, up close. This is the most intimate form of in-person conversation. It feels like someone is talking directly to you.

**When used:**
- Quick exchanges during gameplay (Brod saying "I'm going," Indigo responding)
- In-the-moment reactions where characters are physically together
- Short conversations that do not warrant pulling to third person
- Moments where VR proximity matters -- being face to face with a threat, an ally, or a dying friend

**Soft-lock behavior:**
- The player's view is gently guided toward the speaker (not snapped)
- The player can resist slightly -- look away, glance around -- but the pull brings them back
- The lock releases when the conversation ends or the player needs to act
- The degree of lock varies: casual exchanges are loose, intense moments are tighter

**Format:**
```
[FACE-TO-FACE]
SPEAKER: Dialogue line. (beeps)
SPEAKER: Dialogue line. [VOICED]
(Soft-lock degree: loose / moderate / tight.)
(Trigger condition.)
```

---

### 3. CONVERSATION (Third Person)

The camera pulls out of first person and switches to a **third-person view** of the protagonist. The player sees Indigo from outside -- her body, her posture, her gestures -- talking to another character. This is the game's primary mode for extended dialogue scenes.

The shift to third person signals to the player: **this conversation matters.** It is longer, more deliberate, and carries narrative weight. The camera behaves like an Ocarina of Time conversation -- cutting between characters, holding on reactions, using angle and distance to convey the relationship between the speakers.

Dialogue is delivered through **text and beeps** with directed camera work. Fully voiced lines are rare and marked `[VOICED]`.

**When used:**
- Extended conversations (the puzzle room in Ch3 L6, the separation argument in Ch1 L3)
- Story-critical exchanges where the player needs to see both characters
- Moments where Indigo's body language carries meaning the first-person view would miss
- Crew discussions, planning scenes, emotional beats

**Camera behavior:**
- Over-the-shoulder shots alternating between speakers
- Wide shots establishing the scene and spatial relationship
- Close-ups on faces during emotional beats -- beeps intensify
- The player does not control the camera during third-person conversations

**Format:**
```
[CONVERSATION]
SPEAKER: Dialogue line. (beeps)
SPEAKER: Dialogue line. [VOICED]
(Camera direction.)
(Trigger condition.)
```

---

### 4. GAMEPLAY DIALOGUE

Lines delivered while the player has full control in first-person VR. Characters speak during combat, traversal, or exploration. The player can move, fight, and interact while dialogue plays. Lines must be short enough to land without pausing the action. Delivered as **text and beeps** -- no camera change, no soft-lock.

**When used:**
- Mid-combat callouts
- Traversal observations
- Reactions to events happening in real-time

**Format:**
```
[GAMEPLAY DIALOGUE]
SPEAKER: Dialogue line. (beeps)
(Trigger condition: what causes this line to play.)
```

---

### 5. COMMS

Remote communication via magic signals, radio, or other technology. Characters are not in the same location. Audio-only -- the player hears the voice but does not see the speaker. Typically plays over gameplay.

**When used:**
- Kat feeding intel to the protagonist
- The protagonist contacting crew members remotely
- Intercepted communications
- The radio deception (mimicking the mech pilot)

**Format:**
```
[COMMS]
SPEAKER (via [method]): Dialogue line.
(Trigger condition.)
```

---

### 6. EARRING (Kim's Hint System)

Kim Tiernan speaks to the protagonist through the earring. This is the game's hint system and emotional anchor. Kim's lines range from gameplay hints to personal conversation. The player can trigger earring conversations by tapping the earring, or they fire automatically at key moments.

**Subtypes:**
- **EARRING -- HINT:** Gameplay-relevant information. Kim points the player toward solutions, objectives, or threats.
- **EARRING -- CONVERSATION:** Character dialogue. Kim and the protagonist talk. Emotional, personal, sometimes mundane.
- **EARRING -- REACTION:** Kim reacts to something that just happened. Short, unprompted.

**When used:**
- Player is stuck or idle (hints)
- After major story beats (reactions)
- During quiet moments or travel (conversations)
- Player taps the earring manually (player-initiated)

**Format:**
```
[EARRING -- subtype]
KIM: Dialogue line.
INDIGO: Response line.
(Trigger condition.)
```

---

### 7. BARK

Very short combat lines -- one to five words. Grunts, shouts, callouts. No narrative weight. These are the sounds of a fight, not a conversation. Characters bark during combat without stopping the flow.

**When used:**
- Taking damage
- Landing a hit
- Dodging
- Spotting an enemy
- Enemy callouts to each other

**Format:**
```
[BARK]
SPEAKER: Line.
(Combat trigger.)
```

---

### 8. ENVIRONMENTAL

Text, audio, or visual information embedded in the environment. Not spoken by a character in real-time. Found by the player through exploration.

**When used:**
- Signs, terminals, documents the player reads
- Overheard conversations from NPCs or enemies
- Audio logs or recordings
- Graffiti, markings, or symbols with narrative meaning
- Vennessa's directional paintings

**Format:**
```
[ENVIRONMENTAL]
SOURCE: Content.
(Location / discovery condition.)
```

---

### 9. INTERNAL

The protagonist's inner thoughts. Not spoken aloud. Delivered as internal monologue or narration that only the player hears. Used sparingly -- Indigo is not a narrator. She thinks in fragments, not essays.

**When used:**
- Processing a revelation
- Reacting to something she cannot respond to externally
- Quiet decision-making moments

**Format:**
```
[INTERNAL]
INDIGO (internal): Thought.
(Trigger condition.)
```

---

### 10. UI / SYSTEM

On-screen text, objective updates, tutorial prompts, and system messages. Not character dialogue. Functional game communication.

**When used:**
- Objective updates ("Find Brod Tsumi")
- Tutorial instructions ("Use magic to break the mind control device")
- Save point notifications
- Area/level title cards

**Format:**
```
[UI]
Content.
```

---

## Delivery Rules

### Voice Rarity
1. **Human voice is the exception, not the rule.** The vast majority of dialogue is text + beeps. When a line is fully voiced, mark it `[VOICED]` in the script.
2. **Voiced lines are reserved for peak moments** -- a reveal, a death, a line that defines a character. If every line is voiced, none of them matter. If one line in a scene is voiced, that line is the one the player remembers.
3. **A character's first voiced line should be significant.** The player has been hearing beeps. When the actual voice breaks through, it should land.

### VR Immersion
4. **The player is inside the protagonist's body.** First-person is the default state. Every departure from it (third-person conversation, cutscene) should feel intentional and earned.
5. **Face-to-face soft-locks should feel natural, not forced.** The player should feel guided toward the speaker, not trapped. Loose locks for casual exchanges, tight locks for intense moments.
6. **Third-person conversations signal narrative weight.** Pulling the player out of first person tells them: pay attention. Do not overuse this -- if every conversation is third person, none of them feel important.

### General
7. **No gameplay dialogue line should require the player to stop playing to understand it.** If it plays during active gameplay, it must be short enough to land while the player is fighting, running, or exploring.
8. **Barks never carry story information.** If a player mutes barks, they miss nothing. Barks are sound, not script.
9. **Comms can carry story information** but should be designed so the player can absorb them while playing. Critical intel should be reinforced through gameplay or UI.
10. **Earring conversations are the emotional core.** They should feel like talking to a friend, not receiving a quest log update. Kim's beep pattern should feel warm and familiar -- the sound the player associates with safety.
11. **Cutscenes should be earned.** Every cutscene should deliver something that cannot be delivered through face-to-face, conversation, or gameplay dialogue. The camera leaving first person is a cost. Pay it only when the return is worth it.
12. **Internal lines are rare.** Indigo expresses herself through action, not internal monologue. Use only when there is no other way to communicate her state.

---

## Scene Structure

Each scripted scene in a chapter script should include:
- **Level and moment** -- where in the chapter this occurs
- **Category** -- which delivery method (from above)
- **Characters present** -- who is speaking / who can be heard
- **Trigger** -- what causes the scene to play
- **Lines** -- the actual dialogue or content
- **Notes** -- any direction on tone, pacing, or context
