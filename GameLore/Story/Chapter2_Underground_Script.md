# Chapter 2: Underground -- Script

> *Script categories defined in `Script_Style_Methods.md`*

---

## Level 1: The Underground Route

---

### 1.1 -- Into the Caves

```
[CUTSCENE]
(The protagonist walks deeper into the cave system. Cold river water rushes past her ankles. The walls are wet stone, the ceiling low. The sound of the mountain above is gone -- replaced by dripping water, distant echoes, and the faint rush of the underground river.)

(She pauses. Looks around. Total darkness ahead, faint grey light behind her from the cave mouth. She activates a small magic light on her gear -- just enough to see the next twenty feet.)

(She keeps moving.)
```

---

### 1.2 -- Level Title

```
[UI]
CHAPTER 2 -- UNDERGROUND
Level 1: The Route
```

---

### 1.3 -- Kim on the Caves

```
[EARRING -- CONVERSATION]
KIM: Warmer than the mountain.
INDIGO: Water's still cold.
KIM: Give it time. Kat said it feeds into a volcanic system.
INDIGO: Great. Snow to lava. Love this realm.
KIM: At least you won't be bored.
(Trigger: 30 seconds into traversal, if player is not in combat.)
```

---

### 1.4 -- Kat Check-in

```
[COMMS]
KAT (via magic signal): Indigo, status.

INDIGO: Underground. Moving. River's still cold.

KAT: Good. You're on track. Ys and Robert have the ship nearly ready. I'm coordinating from the rendezvous -- I have eyes on you, Brod, and the surface.

INDIGO: Where's Brod?

KAT: Separate tunnel system, lower elevation. He has Sketch and Tablet back. He's fine. And he found Vennessa.

INDIGO: How?

KAT: She was already down here. She's been leaving directional paintings on the cave walls -- markings to guide anyone back to the rendezvous....

INDIGO: That is smart.

KAT: It's also a trail. Anyone who finds those paintings finds us.
(Trigger: during gameplay traversal.)
```

---

### 1.5 -- Environmental Paintings

```
[ENVIRONMENTAL]
SOURCE: A painting on the cave wall -- a small, precise directional mark. Vennessa's style. An arrow shape worked into a simple image of a ship. It points deeper into the tunnel.
(Location: multiple instances throughout Level 1 caves. The player follows them.)
```

```
[EARRING -- REACTION]
KIM: Mouse can paint.
(Trigger: player examines a painting for the first time.)
```

---

### 1.6 -- The Wolves Enter

```
[COMMS]
KAT (via magic signal, tone shifting): Indigo. I'm picking up movement at the cave entrances above you.

INDIGO: Husky patrols?

KAT: No. Wolf Squad. Jack took the squad underground. They found a surface entrance on the northeast face.

INDIGO: They're in the caves?

KAT: Yes. And Nemo -- the hunter -- he spotted the paintings. They're following Vennessa's trail.

INDIGO: Toward you.

KAT: Toward all of us.
(Trigger: mid-level, after the player has followed several paintings.)
```

**Notes:** The paintings are revealed as a double-edged sword. What guided the crew home is now guiding the wolves. Kat delivers this without blame -- she's stating the tactical reality.

---

### 1.7 -- Kim on the Situation

```
[EARRING -- CONVERSATION]
KIM: So the paintings that were helping you are now helping them.
INDIGO: I heard.
KIM: The mouse did her job. Not her fault the wolves can read.
INDIGO: Didn't say it was.
KIM: You were thinking it.
INDIGO: I was thinking I need to move faster.
KIM: ...That too.
(Trigger: 15 seconds after Kat's warning.)
```

---

## Level 2: Contact Underground

---

### 2.1 -- Level Title

```
[UI]
Level 2: Contact
```

---

### 2.2 -- Kat's Intercept Call

```
[COMMS]
KAT (via magic signal, command voice): Everyone, listen. The Wolf Squad is following Vennessa's paintings toward the rendezvous. If they reach the ship, we lose our way out.

KAT: Robert -- stay with the ship. Do not move it. Do not leave it.

ROBERT (via magic signal, shaky): Copy.

KAT: Everyone else -- Ys, Brod, Vennessa -- move to the midpoint. I'm sending coordinates. We intercept the wolves before they get close.

YS (via magic signal, flat): Understood.

KAT: Indigo, what's your position?

INDIGO: Behind. The mech fight put me behind schedule.

KAT: I know. Get here when you can. We're not waiting.
(Trigger: level start.)
```

**Notes:** Kat runs this. She makes the call, gives orders, coordinates. Ys says one word and means it. Robert's voice should sound like someone trying to hold it together.

---

### 2.3 -- Kim on Being Late

```
[EARRING -- CONVERSATION]
KIM: She said they're not waiting.
INDIGO: I heard her.
KIM: You're not bothered?
INDIGO: I'm bothered I'm not there. I'm not bothered she's not waiting. She's right.
KIM: First time I've heard you say someone else is right.
INDIGO: Don't get used to it...
(Trigger: during traversal toward the midpoint.)
```

---

### 2.4 -- Brod and Vennessa Moving (Parallel)

```
[CUTSCENE -- PARALLEL]
(Brod and Vennessa moving through the caves together. Sketch floats ahead. Tablet walks beside them. They are heading toward Kat's coordinates.)

VENNESSA (quiet, keeping pace): We're about to fight them!

BROD (flat, not looking at her): Yup...

(Beat. Vennessa processes this.)

VENNESSA: You're not worried?

BROD: Used to it....

VENNESSA: Seriously, why?

BROD: I got bills to pay T_T.

VENNESSA: Avolin Credit? 

BROD: .........

VENNESSA: I understand your sadness now.

```

**Notes:** These two are the normal people on the crew, and this conversation sounds like it. No bravado, no battle speeches. Just two people who agree this is insane and are doing it anyway. Vennessa isn't panicking -- she's processing out loud. She asks where Ys is not because she expects a hero, but because it seems like something a captain should be involved in. Brod's answer isn't anger -- it's the resigned exhaustion of someone who has been through this exact cycle with Ys multiple times. "He drags me into something, says almost nothing" -- that's not a complaint about Ys being bad at his job. It's Brod being tired of being in situations he didn't sign up for beyond the paycheck. "You sound tired" / "I am tired" is the most honest beat -- they're in sync. Two normal people reading the same room. His last line -- "draw faster" -- isn't tough. It's the best answer he has, and they both know it's not great.

---

### 2.5 -- The Fight Indigo Misses

```
[COMMS]
(Fragmented signal -- the player hears pieces of the crew's fight while running through the caves toward them. The audio crackles in and out. Beeps convey the chaos.)

KAT (via magic signal, strained): --contact! Five wolves, full squad, tight formation--

(Sounds of combat. Sketch's drawing-casting hum. An explosion.)

BROD (via magic signal, flat): There's a lot of them.

YS (via magic signal, calm): Everyone surround the mouse, Vennessa....DRAW!

(Gunfire. Elpha's assault rifle -- distinct, sustained bursts.)


KAT (via magic signal): --being pushed back. They're cutting us off from the--

(Signal cuts.)
(Trigger: plays during traversal. Player cannot reach the fight in time.)
```


---

### 2.6 -- Arrival at the Aftermath

```
[GAMEPLAY DIALOGUE]
(The protagonist reaches the midpoint. The cave is wrecked -- broken rocks, gouged walls, scorch marks from magic and gunfire. Water from the river is stained with sediment kicked up by the fight. Nobody is here.)

INDIGO: ...I'm too late.
(Trigger: player enters the midpoint area.)
```

```
[EARRING -- REACTION]
KIM: They fought here. The damage goes that way -- deeper, toward the exit.
(Trigger: player examines the aftermath for 5+ seconds.)
```

```
[GAMEPLAY DIALOGUE]
INDIGO: Trail goes east. They were pushed out.
(Trigger: player finds the exit path -- broken terrain leading toward the temple.)
```

---

### 2.7 -- Following the Trail

```
[EARRING -- HINT]
KIM: Follow the damage. Broken walls, blast marks. They went this way.
(Trigger: player idles at the aftermath for 15+ seconds without following the trail.)
```

```
[ENVIRONMENTAL]
SOURCE: Claw marks in the stone -- wolf. Alongside them, Sketch's ink residue on the walls. Both sides left their marks. The fight moved this direction.
(Location: along the trail from the midpoint to the temple exit.)
```

---

## Level 3: The Ship and the Temple

---

### 3.1 -- Level Title

```
[UI]
Level 3: The Ship and the Temple
```

---

### 3.2 -- Robert's Signal

```
[COMMS]
(A magic signal reaches the protagonist. It is rough, unstable, barely held together -- nothing like Kat's clean work. Static hisses through every syllable.)

ROBERT (via magic signal, panicked, barely audible): --anyone-- please-- there's-- the animals, they're-- I'm in the ship, I sealed it, but they're outside and I can't--

(The signal crackles and drops.)

(It comes back.)

ROBERT (via magic signal): --don't know how to do this magic thing-- I just-- please, someone--

(Signal drops again.)
(Trigger: level start, as the player follows the trail toward the temple.)
```

**Notes:** Robert's signal should sound wrong -- like someone who grabbed a tool they don't understand and forced it to work through sheer desperation. She barely knows magic. The signal reflects that.

---

### 3.3 -- The Choice

```
[EARRING -- CONVERSATION]
KIM: That was the mechanic.
INDIGO: I know.
KIM: The crew went that way. She's the other way.
INDIGO: I know.
KIM: ...What are you doing?
(Trigger: player stands at the branching point -- trail to temple vs. Vennessa's paintings leading back to the rendezvous.)
```

```
[INTERNAL]
INDIGO (internal): The crew can fight. She can't.
(Trigger: player turns toward the rendezvous path.)
```

---

### 3.4 -- Reaching the Ship

```
[GAMEPLAY DIALOGUE]
(The protagonist follows Vennessa's directional paintings back toward the rendezvous. The caves narrow, then open into the chamber where the ship is hidden. Wild animals -- underground creatures stirred up by the fighting and volcanic activity -- are circling the ship.)

INDIGO: There she is.
(Trigger: player sees the ship.)
```

```
[BARK]
INDIGO: (exertion -- fighting animals)
INDIGO: Back. Off.
(Trigger: combat with cave animals.)
```

---

### 3.5 -- Robert

```
[FACE-TO-FACE]
(The animals are cleared. The protagonist knocks on the ship hull.)

INDIGO: Robert. Open up. It's me.

(Silence. Then the sound of locks disengaging -- too many locks, all at once. The hatch opens. Robert is inside, crouched behind a console, holding a wrench like a weapon. She is shaking.)

ROBERT: ...You came.

INDIGO: You called. I came. That's how it works.

ROBERT: Animals everywhere. I sealed the hull and waited it out.

INDIGO: Good call.

ROBERT: Where's everyone else?

INDIGO: Working on it.....

INDIGO: I need to go. The others are in trouble.

ROBERT: Wait. Kat's surveillance kit is still hooked up on the bridge. You should look before you run.

INDIGO: Oh yeah!
(Soft-lock degree: moderate.)
(Trigger: player clears the animals and approaches the ship.)
```

**Notes:** Indigo is already moving toward the crew. Robert redirects her -- not with emotion, but with practical sense. She knows the ship better than anyone. This flows directly into the surveillance scene.

---

### 3.6 -- Surveillance

```
[GAMEPLAY DIALOGUE]
(Robert leads the protagonist to the bridge. Kat's surveillance equipment is still running -- magic-enhanced imaging displaying the terrain around the planetoid. Robert taps a few controls, adjusts the range.)

ROBERT: There. That temple by the waterfalls. That's where their signals went.

INDIGO: Pull it up.
(Trigger: player follows Robert to the surveillance station.)
```

```
[CUTSCENE]
(The surveillance feed shows the crew -- Ys, Kat, Brod, Vennessa, Tablet -- fighting the Wolf Squad inside the volcanic cave system. The lava is rising. Everyone is being pushed upward.)

(Camera cuts between angles: Kat's scythe arcs through a wolf that dodges. Tablet moves with precision, cutting, blocking. Ys is a blur -- whatever he does in combat is fast and hard to follow.)

(Vennessa is crouched behind a rock formation, drawing fast -- ink constructs spill off her pages under her own casting and harden into barriers and decoys. She is not fighting. She is producing. Brod fights directly in front of her position -- twin swords, controlled, always angling to keep his body between the wolves and Vennessa.)

(When Elpha's gunfire punches through an ink barrier, Brod doesn't chase the shooter -- he closes the gap on Vennessa's side. Kat calls a flanking route and Ys intercepts before it reaches her. The crew fights as a unit, but the formation has a clear center: Vennessa.)

(The wolves are pressing them hard. Nemo coordinates. Elpha's assault rifle pins the crew behind cover. Bleu handles comms and tech support. Jack is not with them -- he left to pursue the protagonist.)

(The crew is not captured -- but they are losing ground, fighting upward through the volcano with the wolves right behind them. The lava rises below everyone. Both sides are running and fighting at the same time.)

(The protagonist watches through the screen. They are alive. They are fighting. But they are outnumbered, outgunned, and being driven toward the surface.)
```

**Notes:** The player sees the crew in trouble but still fighting -- not captured yet. The capture comes later on the frozen lake. This establishes urgency without jumping ahead. The surveillance is the first time the player sees the crew's formation clearly -- and the center of it is Vennessa. She's not dead weight. She's the supply line. Her drawing magic feeds the crew's defenses. But she can't draw and fight, so the entire crew orients around keeping her alive and producing. Brod is her primary shield. Everyone else fights outward from her position.

**Vennessa casts her own work.** Her constructs come off her own pages by her own hand, start to finish. Sketch has no involvement in Vennessa's magic -- not as an animator, not as a courier, not at all. Sketch only ever touches Brod's drawings.

---

### 3.7 -- After the Surveillance

```
[EARRING -- CONVERSATION]
KIM: They're still fighting.
INDIGO: They're losing.
KIM: What are you going to do?
INDIGO: Get down there.
(Trigger: after the surveillance cutscene ends.)
```

---

## Level 4: Lava Caves

---

### 4.1 -- Level Title

```
[UI]
Level 4: Lava Caves
```

---

### 4.2 -- Descent

```
[GAMEPLAY DIALOGUE]
(The protagonist heads toward the crew's last known position. The caves are hot now -- the river water is warm, uncomfortably so. The stone walls glow faintly with heat. Lava veins run through the rock. The magma is rising from below, pushing her upward.)

INDIGO: It's getting hot down here.
(Trigger: player enters the lava cave environment.)
```

```
[EARRING -- REACTION]
KIM: The earring is... fine. In case you were worried.
INDIGO: Wasn't.
KIM: Good. Because I am.
(Trigger: temperature shift is noticeable in the environment.)
```

---

### 4.3 -- Volcanic Activity

```
[COMMS]
KAT (via magic signal, strained): Indigo-- can you hear me?

INDIGO: Kat. Talk to me.

KAT: The volcanic system beneath you is surging. Magma is rising through the lower tunnels. You are at the lowest point.

INDIGO: I feel it.

KAT: Get higher. Everyone is being pushed up. The wolves, us, everyone. We're fighting and running at the same time.

INDIGO: You okay?

KAT: Holding. Barely. Move.
(Trigger: environmental volcanic activity starts -- tremors, lava visible in lower sections.)
```

**Notes:** Kat is fighting and coordinating at the same time. She's transmitting mid-retreat -- the crew is being pushed upward by both wolves and lava. "Holding. Barely." is honest without being dramatic.

---

### 4.4 -- Bassual Grutie

```
[CUTSCENE]
(The protagonist rounds a corner in the lava caves. The ground is cracking. Magma glows through fissures beneath her feet.)

(A figure drops from a ledge above -- landing between her and the path upward. Small by wolf standards. Petite. Young. But armed and moving like he means it.)

(Bassual Grutie. The youngest wolf. He blocks her path.)

BASSUAL: You're the one.

INDIGO: One of the ones. Which one are you?

BASSUAL: The one between you and the exit.

(He draws his weapon. Behind them both, lava bubbles through a crack in the floor.)
```

---

### 4.5 -- Boss Fight

```
[BARK]
BASSUAL: (aggressive, intense -- he fights harder than his size suggests)
BASSUAL: Stay down!
BASSUAL: I'll bring you in myself!
(Trigger: combat barks throughout the fight.)
```

```
[BARK]
INDIGO: (exertion)
INDIGO: Quick little thing.
(Trigger: after dodging a fast combo from Bassual.)
```

```
[GAMEPLAY DIALOGUE]
(Lava surges through a fissure. The arena shrinks.)
INDIGO: Ground's running out.
(Trigger: lava hazard reduces the fighting area mid-fight.)
```

```
[EARRING -- HINT]
KIM: The lava's cutting off his retreat too. He can't back up forever.
(Trigger: Bassual's health below 50%.)
```

---

### 4.6 -- Bassual Falls

```
[CUTSCENE]
(Bassual is beaten. He's on the ground, weapon knocked away. The lava is close -- fissures glowing behind him. He is conscious. He is looking up at her.)

(He does not beg. He does not surrender. He reaches for his comms.)

BASSUAL (into comms, breathing hard): ...Jack. I engaged the target solo. She's... she's better than the brief said.

(He coughs.)

BASSUAL: I lost.

(He drops the comms. His eyes don't leave the protagonist. There is no fear in them. Just the look of someone who gave everything and came up short.)

(The protagonist leaves him. She moves upward, toward higher ground, away from the rising lava.)

(Behind her, the caves continue to flood.)
```

**Notes:** Bassual doesn't die on screen. The lava and the caves take him. The player understands what happens without seeing it. His last act is reporting his failure -- he's a soldier to the end.

---

### 4.7 -- Kim After Bassual

```
[EARRING -- CONVERSATION]
KIM: ...That was cold.
INDIGO: He was in the way.
KIM: I know. Just... not used to seeing you like that.
INDIGO: Like what?
KIM: Like you didn't care.
INDIGO: The taco's got hot sauce, Kim. Don't let the wrap fool you.
KIM: ...I forget sometimes. You don't look like the type. You're cute not scary
INDIGO: ........
(Trigger: 20 seconds after leaving Bassual, during the climb upward.)
```

**Notes:** Kim is unsettled by how easily she left Bassual to die. "Not used to seeing you like that" -- he knows she's capable, but watching it still hits different. "The taco's got hot sauce" is Indigo's way of saying don't judge the inside by the outside. Kim's "you're cute not scary" disarms her completely -- only he would say something affectionate right after watching her leave someone to die. Indigo's silence is the punchline. She can shut down anyone in this game except Kim.

---

### 4.8 -- Jack's Decision

```
[ENVIRONMENTAL]
(Overheard Wolf Squad comms -- the player passes near enough to catch a transmission.)

JACK (overheard, comms): Bassual reported in. He engaged and lost. The target beat him in the lava caves.

NEMO (overheard, comms): Orders?

JACK: I'm going after her myself. The three of you handle the crew. I'll find her below.

NEMO: Understood.

JACK: Nemo. If I'm not back by the time the lava settles... don't come looking.

NEMO: ...Understood.
(Location: near a comms relay or signal leak point.)
```

**Notes:** Bassual was sent to **locate** the missing crew member and report back -- not to engage. He disobeyed. He fought her solo and died for it. Jack doesn't say this out loud, but the player should feel it: his youngest wolf ignored a direct order and it killed him.

This is the seed for Chapter 4. Jack himself has a long history of disobedience -- he would normally tell any superior to shove it. But in Chapter 4, when the tiger captain berates him and orders him to stand down, Jack actually listens. Not because the tiger captain earned his respect. Because Bassual's death is a fresh scar. The tiger captain's words -- "you're too weak, you'd just get in the way" -- land on top of the guilt Jack is already carrying. He gave Bassual an order. Bassual didn't listen. Bassual is dead. Now someone is giving Jack an order. The wound is too raw.

---

## Level 5: Frozen Lake

---

### 5.1 -- Level Title

```
[UI]
Level 5: Frozen Lake
```

---

### 5.2 -- Surface

```
[GAMEPLAY DIALOGUE]
(The protagonist emerges from the caves onto the surface. The temperature drops violently -- from volcanic heat to freezing cold. A frozen lake stretches out before her. Ice. Wind. Grey sky.)

INDIGO: ...That's a shift.
(Trigger: player exits the cave system.)
```

```
[EARRING -- REACTION]
KIM: From lava to ice. This realm has range.
(Trigger: immediately after surfacing.)
```

---

### 5.3 -- Spotting the Crew

```
[GAMEPLAY DIALOGUE]
(The protagonist reaches a vantage point overlooking the frozen lake. In the distance -- the crew and the Wolf Squad are fighting on the ice. Three wolves: Nemo, Elpha, Bleu. No Jack. The crew is holding but losing ground.)

(Even from a distance, the formation is visible: Brod and Tablet anchoring the front, Kat weaving between engagements, Ys cutting off angles. Vennessa is behind them all, still drawing -- ink constructs rising off her own pages and rushing into the fight. The crew has been protecting her the entire retreat.)

INDIGO: There they are. Still fighting. Three wolves, no Jack.
(Trigger: player reaches the observation point.)
```

```
[EARRING -- HINT]
KIM: Three wolves, no leader. They might hold without you.
INDIGO: They won't. I need to get to the ship.
(Trigger: player observes the fight for 5+ seconds.)
```

---

### 5.4 -- Jack Underground

```
[CUTSCENE]
(The protagonist turns away from the frozen lake. She is heading for the ship -- for Robert, for a plan. She moves toward a cave entrance.)

(Movement. Below. Inside the cave she just left.)

(Jack emerges. He does not shout. He does not announce himself. He simply appears -- stepping out of the dark, weapons ready, eyes locked on her.)

INDIGO: Another one....

JACK: Bassual was twenty-two.

(Beat.)

INDIGO: Rover should've backed off.

JACK: ...I'm going to enjoy this.

(He attacks.)
```

---

### 5.5 -- Underground Skirmish

```
[BARK]
JACK: (precise, controlled -- no wasted movement)
JACK: Faster than the report said.
INDIGO: Reports lie.
(Trigger: first exchange.)
```

```
[GAMEPLAY DIALOGUE]
(This is not a boss fight. It is a skirmish -- fast, violent, and the protagonist cannot win. Jack is better. She needs to survive and escape.)

INDIGO: Can't beat him straight. Not here.
(Trigger: player takes significant damage early in the fight.)
```

```
[EARRING -- HINT]
KIM: The tunnel behind him -- it's unstable. The lava weakened the supports. You don't need to beat him. You need to move him.
(Trigger: player health drops below 60%.)
```

---

### 5.6 -- The Trap

```
[CUTSCENE]
(The protagonist maneuvers -- retreating, leading Jack deeper into an unstable section of the cave. The walls are cracked. Support columns are fractured from the volcanic activity.)

(She strikes a support column with her pike. Not Jack -- the cave itself.)

(The ceiling groans. Rocks shift. Jack sees it.)

(He has a split second to decide: push forward through the collapse and pursue her, or retreat.)

(He retreats.)

(The tunnel collapses between them. Dust. Silence. She is on one side. He is on the other. He is not hurt. He is not defeated. He is delayed -- forced to find another exit.)

INDIGO (breathing hard, to herself): ...Bought some time.
```


---

### 5.7 -- The Crew on the Ice

```
[COMMS]
(The protagonist picks up a transmission -- Wolf Squad comms she can intercept thanks to the mech pilot's radio equipment from Chapter 1.)

NEMO (overheard, comms): Prisoners are secure. The ice is holding. Elpha, watch the northern approach.

ELPHA (overheard, comms): Covered.

NEMO: Bleu, status on Jack?

BLEU (overheard, comms): Last check-in -- he went below after the target. No update since.

NEMO: ...Give him time. He'll surface.
(Trigger: during traversal toward the ship.)
```

---

### 5.8 -- Kat's Gambit

```
[COMMS]
(Intercepted transmission -- the protagonist hears this through the captured comms equipment.)

NEMO (overheard): The prisoners are more trouble than they're worth. Twice now. I say we end it.

ELPHA (overheard): Standing orders say bring them in. But standing orders came before they killed Bassual.

NEMO: Jack isn't answering. If he's dead too--

KAT (overheard, calm and clear -- speaking to the wolves directly): You kill us, you lose the prophecy child.

NEMO: ...What did you say?

KAT: The girl your leader is chasing right now. She is the prophecy child. The one both armies are looking for. We are her crew. Kill us, and you have nothing to trade, nothing to bargain with, and nothing to show your command except four dead prisoners and a target that got away.

(Silence.)

NEMO: ...Bleu. Report up the chain. Tell them we have the prophecy child's crew. Request instructions.

BLEU: Copying now.
(Trigger: plays during traversal. The player hears the crew's lives being saved in real time.)
```

**Notes:** Kat saves the crew from execution with information and timing. She does not beg. She does not threaten. She gives them a reason to keep the crew alive that serves the wolves' interests. This is Kat at her most dangerous -- she's tied up and she's still the smartest person in the conversation.

---

### 5.9 -- Kim on Kat

```
[EARRING -- REACTION]
KIM: Storm's picking up out there.
INDIGO: I noticed.
KIM: You should hurry.
(Trigger: after Kat's gambit plays.)
```

---

## Level 6: Jack Falls

---

### 6.1 -- Level Title

```
[UI]
Level 6: Jack Falls
```

---

### 6.2 -- Observation

```
[GAMEPLAY DIALOGUE]
(The protagonist surfaces and reaches a vantage point. She can see the frozen lake below. The crew is still there -- bound, guarded by three wolves. No one is being killed. Kat's gambit worked.)

INDIGO: They're alive. Kat bought time.
(Trigger: player reaches the overlook.)
```

```
[EARRING -- CONVERSATION]
KIM: You can see them from here. Three wolves, four prisoners, open ice.
INDIGO: I can't take three wolves and free them at the same time.
KIM: No. But you have the ship.
INDIGO: I have the ship. And I have Robert.
KIM: ...Generous use of the word "have."
INDIGO: She'll be fine.
(Trigger: player observes the lake for 5+ seconds.)
```

**Notes:** Kim's "generous use of the word 'have'" -- Robert as a tactical asset is questionable. But Indigo needs the ship and Robert is the one who flies it.

---

### 6.3 -- Finding Robert

```
[COMMS]
(The protagonist tries to locate Robert. No clean signal -- Robert is nearly magicless. But the protagonist can sense her location: the ship has been moved to the top of a giant tower, high above the frozen terrain.)

INDIGO: She moved it. Smart. High ground.
(Trigger: player uses magic to locate the ship.)
```

```
[EARRING -- HINT]
KIM: The tower. She took the ship up there. Safer than the caves with the lava rising.
(Trigger: player orients toward the tower.)
```

---

### 6.4 -- Jack Returns

```
[CUTSCENE]
(The protagonist is moving toward the tower. Open terrain -- frozen ground, scattered rocks, wind picking up. The sky is darkening. A storm is building in the distance. A white tornado churns on the horizon.)

(Footsteps behind her. Heavy. Deliberate.)

(She turns.)

(Jack. He surfaced from a different cave exit. He is not running. He is walking. His weapons are out. His face is set.)

JACK: I went back for Bassual.

(Beat.)

JACK: He didn't make it.

(Beat.)

INDIGO: ...

JACK: Nemo tells me the rabbit woman says you're the prophecy child. Both armies are going to be very interested in that. My squad can handle the prisoners. Higher command is being notified. And you--

(He raises his weapon.)

JACK: --are mine.

INDIGO: Come get me!
```

---

### 6.5 -- Boss Fight

```
[BARK]
JACK: (controlled, relentless -- not angry, focused)
JACK: You're good. Not good enough.
(Trigger: first exchange.)
```

```
[BARK]
INDIGO: (exertion -- this fight is harder than anything in the chapter)
INDIGO: Tch.
(Trigger: taking a clean hit from Jack.)
```

```
[GAMEPLAY DIALOGUE]
(Lightning strikes the frozen ground nearby. Wind howls. Snow whips across the arena. The white tornado dominates the background.)

INDIGO: Storm's getting worse.
(Trigger: first weather hazard event.)
```

```
[EARRING -- HINT]
KIM: He overcommits on the third swing in his combo. Every time. Wait for it.
(Trigger: player has been hit by Jack's full combo twice.)
```

```
[BARK]
JACK: You beat Bassual in the lava. Let's see what you do out here.
(Trigger: mid-fight, when player lands a strong hit.)
```

```
[GAMEPLAY DIALOGUE]
(The tornado shifts. The wind pulls at both fighters. The ice cracks beneath their feet.)

INDIGO: Use it. Everything is a weapon.
(Trigger: environmental hazard creates an opening.)
```

```
[EARRING -- REACTION]
KIM: He's slowing. You're not. Keep pressing.
(Trigger: Jack's health below 30%.)
```

---

### 6.6 -- Jack Falls

```
[CUTSCENE]
(The storm rages. Lightning. Wind. The white tornado is closer now -- pulling at debris, bending the air.)

(Jack swings. The protagonist ducks. She counters -- pike strike to his ribs, then a spinning kick that sends him sliding across the ice.)

(He tries to stand. She's already there. Pike at his throat.)

(He looks up at her. Breathing hard. Blood on his face. His weapon is out of reach.)

JACK: ...Do it.

INDIGO: (smiling, almost cheerful) Nah.

JACK: Kill me or I'll--

INDIGO: You'll what? Chase me again?

(She picks up a stone from the frozen ground. She holds it in front of him -- then channels magic through it. The stone softens, bends, and wraps around his wrists like clay. It hardens instantly. Magical restraints -- solid rock fused to his joints.)

INDIGO: How's that feel, Rover? Sit, Fido. Good boy.

(Jack pulls against the stone. It doesn't budge.)

JACK: ...You think this holds me?

INDIGO: Absolutely yes....
```

**Notes:** Indigo captures, not kills. Same pattern as Chapter 1 -- intelligence over violence. Jack expects execution. He gets something worse: being used.

---

### 6.7 -- Reaching the Tower

```
[GAMEPLAY DIALOGUE]
(The protagonist drags the bound Jack toward the tower. The storm is fading. The frozen lake stretches behind them.)

INDIGO: Robert. I'm coming up. Open the hatch.

ROBERT (via magic signal, surprised): You're-- okay. Okay. Opening now.
(Trigger: player reaches the tower base.)
```

---

### 6.8 -- Kim at the End

```
[EARRING -- CONVERSATION]
(The protagonist stands at the top of the tower. The ship is behind her. Jack is bound at her feet. The frozen lake stretches below -- and on it, her crew is still in chains, guarded by three wolves.)

KIM: You have him. The leader.
INDIGO: I have him.
KIM: And the crew?
INDIGO: Still down there. Three wolves.
KIM: What are you going to do?
INDIGO: ...I'll figure it out. I always do......
KIM: Yeah. You do.....
(Trigger: player arrives at the ship with Jack. Plays before the chapter end screen.)
```

**Notes:** Chapter 2 ends the same way Chapter 1 did -- on Kim. The earring is still warm. He's still here. The crew is not free yet. But she has the piece she needs.

---

### 6.9 -- Chapter End

```
[UI]
CHAPTER 2 -- COMPLETE

[CUTSCENE]
(Wide shot: the protagonist standing at the edge of the tower, looking down at the frozen lake. Wind in her hair. Pike on her back. The captured Jack behind her. Below, the crew is still in enemy hands.)

(She does not look worried. She looks like she's planning.)

(Black.)
```

---

## Script Notes

### Voice Direction -- IndigoLLiy
- Same as Chapter 1. Short, direct, cocky in conversation and cold in combat.
- This chapter adds a new layer: she is behind schedule, playing catch-up, and she knows it. Her frustration is controlled -- she doesn't explode, she moves faster.
- Her moment with Robert is the closest she gets to softness with a non-Kim character. "You were surviving" is practical compassion.

### Voice Direction -- Kim
- Same as Chapter 1. Calm observer, gentle humor.
- He carries more weight in this chapter: "He was young" after Bassual is Kim at his most human. He doesn't judge. He notices.
- His line about Kat -- "Remind me not to argue with her" -- is earned humor after a tense moment.

### Voice Direction -- Kat
- Kat runs this chapter. She coordinates, intercepts, and saves the crew from execution while tied up.
- She does not raise her voice. She does not beg. Even under threat, she is the smartest person in the room.
- "They didn't search well enough" -- smug but earned. "Don't call me that" -- she maintains boundaries even under pressure.

### Voice Direction -- Brod
- Minimal lines in this chapter, but his actions speak. He fights in front of Vennessa the entire time -- twin swords, controlled, never overextending. He doesn't tell her to hide or stay safe. He tells her to keep drawing. That's his version of trust.
- "There's a lot of them" -- flat, honest assessment. No drama. "Don't try. Draw." -- not comfort, instruction. He treats her like a teammate, not a liability.

### Voice Direction -- Captain Ys
- One word: "Understood." Then "Hold." Ys does not talk when action is needed. His silence is its own character.

### Voice Direction -- Robert (Mechanic)
- Terrified. Genuine. She is not pretending to be brave.
- Her signal is broken because she is broken -- she barely knows magic and forced a transmission through willpower.
- "You came" -- two words that carry all her relief.

### Voice Direction -- Jack
- Professional. Controlled. Even after losing Bassual, he does not rage -- he channels it into focus.
- "Bassual was twenty-two" is not an accusation. It's information. He's telling Indigo what she took.
- "Do it" when she has him pinned is not surrender -- it's testing her. When she refuses, he recalibrates.

### Voice Direction -- Bassual Grutie
- Intense. Young. Fights like he has something to prove.
- His final comms report is professional -- even dying, he is a soldier. No self-pity, no begging.

### Voice Direction -- Nemo Crowns
- Efficient. Professional. Runs the squad in Jack's absence without hesitation.
- He considers executing the prisoners from a tactical standpoint, not cruelty. When Kat gives him a reason not to, he takes it.

### Voice Direction -- Wolf Squad (General)
- Professional military comms. Short, clear, no personality. They are a unit, not individuals -- except when their lines reveal character (Nemo's pragmatism, Elpha's agreement with execution, Bleu's efficiency).

### Bark Volume
- Wolf Squad barks should sound elite -- controlled, coordinated, never panicked.
- Bassual's combat barks are the exception: intense, aggressive, the youngest wolf trying to prove himself.
- Indigo's combat barks remain minimal. She talks less as the fights get harder.
