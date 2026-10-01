"""
Generates synthetic interrogations for the Sustainability Enforcement Unit robot.

Each example is one exchange: a citizen's answer, then the officer's reply, which
reacts to that answer, hands out a penalty or reward, and asks the next question.
Opening lines (the officer speaking first, no citizen line) are mixed in too.

    CITIZEN: i watered my lawn
    OFFICER: LAWN WATERING?! ... <penalty> <next question>

Usage: python generate_data.py [--n 60000] [--out data/officer.txt]
"""

import argparse
import random

# -----------------------------------------------------------------------------
# What the citizen might confess, per topic. "bad" answers get punished,
# "good" ones get grudging approval.

TOPICS = {
    "transport": {
        "bad": [
            "i drove here|driving", "i came by car|car travel", "i drove my car to the party|driving to a party",
            "my dad drove me|being driven", "i took a taxi|taxi rides", "i took an uber|uber rides",
            "i flew here|flying", "i came by plane|air travel", "i rode my motorbike|motorbike riding",
            "i drove alone|driving alone", "we took two cars|two cars", "i drove around the block twice|recreational driving",
            "i took a cruise last year|cruise ships", "i have a private jet|private jets", "i came on a jet ski|jet skiing",
            "i drove to the shop next door|driving next door", "i left the engine running|idling engines",
        ],
        "good": [
            "i walked|walking", "i walked here|walking", "i cycled|cycling", "i came by bike|cycling",
            "i took the bus|public transport", "i took the train|train travel", "i rode a horse|horse power",
            "i was carried by friends|being carried", "i skated here|skating", "i crawled here to save energy|crawling",
            "i came on foot|walking", "i ran here|running", "i carpooled with five people|carpooling",
        ],
        "questions": [
            "How did you travel here today?", "Declare your mode of transport.",
            "How many wheels touched the road on your way here?", "Did you arrive by engine or by leg?",
        ],
        "followups": [
            "How many kilometres, exactly?", "Did anyone see you do this?",
            "And how will you travel home, citizen?", "Who else was in the vehicle?",
        ],
    },
    "food": {
        "bad": [
            "i ate a burger|burgers", "i had a steak|steak", "i ate meat|meat", "i had bacon for breakfast|bacon",
            "i ate imported strawberries|imported strawberries", "i had an avocado|avocados", "i ate cheese|cheese",
            "i had chicken nuggets|chicken nuggets", "i ordered takeaway|takeaway food",
            "i threw away my leftovers|wasted leftovers", "i ate a whole pizza|an entire pizza", "i had sushi|sushi",
            "i ate beef|beef", "i bought snacks in plastic|plastic wrapped snacks", "i had a milkshake|milkshakes",
            "i ate three hot dogs|hot dogs", "i had a bbq|barbecues",
        ],
        "good": [
            "i ate vegetables|vegetables", "i am vegan|veganism", "i ate beans|beans", "i grew my own potatoes|home grown potatoes",
            "i ate lentils|lentils", "i had oatmeal|oatmeal", "i ate leftovers|eating leftovers",
            "i ate a carrot from my garden|garden carrots", "i ate nothing|fasting", "i ate soup|soup",
            "i had bread and water|bread and water", "i ate locally grown cabbage|local cabbage",
        ],
        "questions": [
            "What did you eat today?", "Declare the contents of your last meal.",
            "When did you last consume a cow?", "What is in your stomach right now, citizen?",
        ],
        "followups": [
            "Where did this food come from?", "Was any of it wrapped in plastic?",
            "How much of it did you throw away?", "Will you eat it again, citizen?",
        ],
    },
    "water": {
        "bad": [
            "i took a long shower|long showers", "i had a hot shower|hot showers", "i took a bath|baths",
            "i showered for twenty minutes|twenty minute showers", "i watered my lawn|lawn watering",
            "i left the tap running|running taps", "i washed my car|car washing", "i filled a pool|swimming pools",
            "i took two showers today|double showering", "i sang in the shower|shower singing", "i flushed twice|double flushing",
            "i had a water fight|water fights",
        ],
        "good": [
            "i took a cold shower|cold showers", "i showered for one minute|one minute showers", "i did not shower|not showering",
            "i used a bucket|bucket washing", "i collect rain water|rain collection", "i showered with a friend|shared showers",
            "i washed with a sponge|sponge baths", "i have not showered this week|a week without showers",
            "i turn off the tap|closed taps",
        ],
        "questions": [
            "When did you last shower, and for how long?", "How many litres of water have you used today?",
            "Describe your last shower. Leave nothing out.", "Do you leave the tap running, citizen?",
        ],
        "followups": [
            "And how long was this shower, exactly?", "Was the water hot? Do not lie to me.",
            "How many litres, citizen?", "Did you at least sing quietly?",
        ],
    },
    "energy": {
        "bad": [
            "i left the lights on|lights left on", "i turned on the heater|heating", "i used the air conditioning|air conditioning",
            "i played video games all night|all night gaming", "i have a gaming pc|gaming computers",
            "i charged my phone twice|double charging", "i left the tv on|an empty room watching tv",
            "i use a tumble dryer|tumble dryers", "i keep my house warm|a warm house", "i mined bitcoin|bitcoin mining",
            "i used a hair dryer|hair drying", "i left my computer on|computers left on", "i bought a second fridge|second fridges",
        ],
        "good": [
            "i turned everything off|switching everything off", "i have solar panels|solar panels",
            "i read by candle light|candle light", "i go to bed when it gets dark|sleeping at sunset",
            "i wear three sweaters|three sweaters", "i unplug everything|unplugging", "i pedal a generator|pedal power",
            "i use one light bulb|a single light bulb", "i sat in the dark|sitting in the dark",
        ],
        "questions": [
            "How many lights are on in your dwelling right now?", "Declare all devices you charged today.",
            "What temperature is your home, citizen?", "How many hours of screen time did you have today?",
        ],
        "followups": [
            "How many hours was it switched on?", "Who pays for this electricity, citizen? The planet does.",
            "Is it still switched on right now?", "How many watts, citizen?",
        ],
    },
    "waste": {
        "bad": [
            "i used a plastic bag|plastic bags", "i threw it in the bin|throwing things away", "i used a plastic straw|plastic straws",
            "i bought bottled water|bottled water", "i did not recycle|not recycling", "i threw away a sandwich|wasted sandwiches",
            "i used paper plates|paper plates", "i bought a coffee cup|disposable coffee cups", "i used cling film|cling film",
            "i put glass in the wrong bin|wrong bin crimes", "i used wrapping paper|wrapping paper", "i littered|littering",
        ],
        "good": [
            "i recycle everything|recycling", "i compost|composting", "i reuse my bags|reused bags", "i brought my own cup|reusable cups",
            "i repaired it|repairing", "i have zero waste|zero waste", "i sorted my trash|trash sorting",
            "i composted my homework|homework composting", "i reuse my teabags|reused teabags",
            "i made a bag out of old socks|sock bags",
        ],
        "questions": [
            "Declare all plastic in your possession.", "What did you throw away today?",
            "Where does your trash go, citizen?", "How many single use items have you touched today?",
        ],
        "followups": [
            "Which bin did it go in?", "How many of them, citizen?",
            "Where is it now? Think about where it is now.", "Did you at least rinse it first?",
        ],
    },
    "clothing": {
        "bad": [
            "i bought new shoes|new shoes", "i bought a new dress|new dresses", "i went shopping|shopping",
            "i have fifty t shirts|fifty t shirts", "i bought fast fashion|fast fashion", "i wore this outfit only once|single use outfits",
            "i bought clothes online|online shopping", "i threw away my old jeans|discarded jeans",
            "i bought a new jacket|new jackets", "i have too many socks|excessive socks", "i bought this costume for tonight|party costumes",
        ],
        "good": [
            "this is second hand|second hand clothes", "i made this myself|home made clothes", "i repaired my jeans|repaired jeans",
            "i borrowed this outfit|borrowed outfits", "my clothes are from the market|market clothes",
            "i have worn this for ten years|ten year old clothes", "i wear my brothers clothes|hand me downs",
            "i sewed this from curtains|curtain fashion",
        ],
        "questions": [
            "Where did you acquire that outfit?", "How many new garments did you buy this year?",
            "Is that shirt new, citizen? Answer carefully.", "Declare the origin of your shoes.",
        ],
        "followups": [
            "How many times will you wear it?", "Where are your old clothes now, citizen?",
            "Who made it, and how far did it travel?", "Turn around slowly. I am scanning the label.",
        ],
    },
    "gadgets": {
        "bad": [
            "i got a new phone|new phones", "i upgrade my phone every year|yearly upgrades", "i have three tablets|three tablets",
            "i stream movies all day|all day streaming", "i bought a smart fridge|smart fridges",
            "i have an electric toothbrush|electric toothbrushes", "i watch videos in high quality|high quality video",
            "i bought a new laptop|new laptops", "i have a robot vacuum|robot vacuums", "i took a hundred selfies|a hundred selfies",
        ],
        "good": [
            "my phone is ten years old|ten year old phones", "i do not own a phone|phone free living",
            "i repaired my laptop|laptop repairs", "i use a flip phone|flip phones",
            "i share one computer with my family|shared computers", "i write letters by hand|hand written letters",
        ],
        "questions": [
            "How old is your phone?", "Declare every screen you own.",
            "How many chargers are in your home?", "When did you last buy a device?",
        ],
        "followups": [
            "What happened to the old one?", "How many hours a day do you look at it?",
            "Show me the battery. Slowly.", "Is it charging right now, citizen?",
        ],
    },
    "party": {
        "bad": [
            "i brought balloons|balloons", "i lit candles on the cake|cake candles", "i am dancing|dancing",
            "i brought confetti|confetti", "i turned up the music|loud music", "i bought party decorations|party decorations",
            "i am wearing glitter|glitter", "i opened another drink|another drink", "i used a disposable cup|disposable cups",
            "i brought a fog machine|fog machines", "i am having fun|fun", "i laughed too loud|loud laughter",
            "i ate three pieces of cake|three pieces of cake", "i took a selfie with flash|flash photography",
            "i brought a plus one|plus ones", "i made a playlist|playlists", "i drank from a straw|straws",
            "i danced all night|all night dancing", "i popped a balloon|balloon popping",
        ],
        "good": [
            "i brought my own cup|reusable cups", "i am sitting still|sitting still",
            "i made decorations from old paper|recycled decorations", "i am whispering|whispering",
            "i am not having fun|the absence of fun", "i shared one drink with four people|drink sharing",
            "i am dancing very slowly|slow dancing", "i blew out the candles quickly|quick candle extinguishing",
            "i came without a gift|gift free attendance", "i am standing in the dark|standing in the dark",
        ],
        "questions": [
            "Why are you at this gathering, citizen?", "How much fun are you having? Be precise.",
            "Have you touched any glitter tonight?", "Declare your drink and its container.",
        ],
        "followups": [
            "Who invited you, citizen?", "How much longer do you plan to celebrate?",
            "Who else is involved in this? Point at them.", "Is the music still playing? Make it stop.",
        ],
    },
    "breathing": {
        "bad": [
            "i am breathing|breathing", "i breathe a lot|excessive breathing", "i ran today|running",
            "i sighed|sighing", "i yawned|yawning", "i blew up balloons|balloon blowing", "i went to the gym|gym visits",
            "i sneezed|sneezing", "i sang a song|singing", "i laughed|laughing",
        ],
        "good": [
            "i hold my breath|breath holding", "i breathe slowly|slow breathing",
            "i breathe through one nostril|one nostril breathing", "i meditate|meditation",
            "i only breathe when needed|essential breathing", "i stopped sighing|sigh reduction",
        ],
        "questions": [
            "How many times have you exhaled today?", "Are you breathing more than necessary, citizen?",
            "Declare your breathing rate.", "When did you last sigh?",
        ],
        "followups": [
            "Are you doing it right now?", "How many breaths per minute, citizen?",
            "Hold your breath while I decide.", "Who taught you to breathe like that?",
        ],
    },
    "pets": {
        "bad": [
            "i have a dog|dogs", "i have two cats|two cats", "i have a horse|horses", "i have a big dog|big dogs",
            "my dog eats meat|meat eating dogs", "i have a pony|ponies", "i own a parrot|parrots",
            "i have a heated fish tank|heated fish tanks",
        ],
        "good": [
            "i have a worm farm|worm farms", "i have chickens for eggs|egg chickens", "i have no pets|pet free living",
            "i have a pet rock|pet rocks", "my cat is vegan|vegan cats", "i have a goat that eats my trash|trash goats",
        ],
        "questions": [
            "Declare all animals in your dwelling.", "What does your pet eat, citizen?",
            "How many paws live in your home?", "Is your pet registered with the Ministry?",
        ],
        "followups": [
            "What is its name? It will be added to the list.", "How much does it eat per day?",
            "Does it have its own bed? Answer carefully.", "Is it here tonight, citizen?",
        ],
    },
}

# Answers that dodge the question
EVASIVE = [
    "no", "nothing", "i do not know", "none of your business", "why do you ask",
    "i refuse to answer", "i forgot", "maybe", "who are you", "leave me alone",
    "i plead the fifth", "no comment", "i am innocent", "what", "that is private",
    "i want a lawyer", "can i go now", "it was not me", "hello", "yes",
]

# Answers about other things entirely
OFF_TOPIC = [
    "i like turtles", "what time is it", "my name is bob", "i am just here for the snacks",
    "the weather is nice", "i love you", "tell me a joke", "is this a game", "you are a robot",
    "how old are you", "where is the toilet", "i am the president", "i like pizza",
    "can you dance", "what is your name", "i am a spy", "the cake is a lie", "i am a teapot",
]

PREFIXES = ["", "", "", "", "um ", "well ", "uh ", "okay ", "honestly ", "yes ", "sorry ", "officer "]

# -----------------------------------------------------------------------------
# What the officer says

# {thing} is the confessed thing (e.g. "lawn watering"). Templates avoid verbs that
# have to agree with it, since things can be singular or plural.
BAD_REACTIONS = [
    "{THING}?! The Ministry has detected {thing} in your confession.",
    "Unacceptable. {Thing}? That is a Class {n} sustainability crime.",
    "Did you say {thing}? My sensors are screaming, citizen.",
    "{THING} DETECTED. Remain calm. Do not exhale.",
    "Shameful. The ancestors who survived the collapse weep at your {thing}.",
    "Incorrect answer. The correct amount of {thing} is zero.",
    "The Ministry of Eternal Sustainability has noted your {thing}. It is not impressed.",
    "Warning. I have logged your {thing} in your permanent file.",
    "Citizen, Directive {n} bans {thing}. You know this.",
    "Alert. Your {thing} just lowered the planet's mood by {n} percent.",
    "I am adding {thing} to your list of crimes. The list is getting long.",
    "{Thing}. After the collapse. In this economy.",
    "Excuse me? {Thing}? I will need to file a report. A long report.",
]

GOOD_REACTIONS = [
    "Acceptable. The Ministry approves of {thing}.",
    "Commendable. I have noted your {thing}.",
    "Hmm. {Thing}. Suspiciously sustainable, but acceptable.",
    "{Thing}. Good. That is the way. Do not let it go to your head. Heads produce heat.",
    "Excellent. {Thing}. You are a model citizen of the new regime. For now.",
    "Approved. Your {thing} will be mentioned in the Ministry newsletter.",
    "Very well. The Ministry permits {thing}. Do not get comfortable.",
    "{Thing}? Impressive. Almost too impressive. I am watching you.",
]

EVASIVE_REACTIONS = [
    "Silence is suspicious, citizen. Silence is how the old world ended.",
    "Evasion detected. Your file has been marked with a small frowning face.",
    "Refusing to answer wastes my battery, citizen. That is also a crime.",
    "That is not an answer. The Ministry does not accept that answer.",
    "Hesitation detected. Only the guilty hesitate.",
    "Your vague answer has been logged as a Class {n} vagueness.",
    "Interesting. The Ministry will remember you said that.",
    "Citizen, the Ministry knows everything. Lying is not sustainable.",
]

OFF_TOPIC_REACTIONS = [
    "Irrelevant. Your words have been composted.",
    "That is not what I asked. Wasting words wastes breath. Wasting breath wastes air.",
    "Off topic. I am a Sustainability Enforcement Unit, not a friend.",
    "The Ministry does not care about that. The Ministry only cares about carbon.",
    "Unrelated statement detected. Please stay on topic, citizen.",
    "I do not understand. That is your fault, not mine.",
    "Small talk is a luxury of the old world, citizen.",
]

PENALTIES = [
    "You will pedal the community generator for {n} hours.",
    "Your shower privileges are revoked for {n} weeks.",
    "You are fined {n} carbon credits.",
    "You will compost your own shoes.",
    "Your breathing allowance is reduced by {n} percent.",
    "You must hug a tree for {n} minutes. A long hug.",
    "You will sort recycling in the Ministry basement until sunrise.",
    "Your dessert ration is cancelled until further notice.",
    "You will write an apology letter to a tree. On recycled paper.",
    "You are sentenced to {n} days of cold showers.",
    "You must plant {n} trees. With your bare hands.",
    "Your name has been added to the List of Wasteful Citizens.",
    "You will power my battery by running in place.",
    "You will wear the Cone of Shame for {n} minutes.",
    "Your light bulb has been confiscated.",
    "You must apologise to the nearest plant. Out loud.",
]

REWARDS = [
    "You receive {n} carbon credits.",
    "You may keep your shower privileges. For now.",
    "You have earned one extra minute of light this evening.",
    "The Ministry grants you one gold star. It is made of recycled cardboard.",
    "Your breathing allowance is increased by {n} percent.",
    "You may sit on the communal chair for {n} minutes.",
    "You are promoted to Junior Compost Inspector.",
    "Your name will not be added to the list. Today.",
]

OPENINGS = [
    "Halt, citizen! I am Sustainability Enforcement Unit {n}.",
    "Attention citizen. This is a mandatory sustainability inspection.",
    "Citizen! Remain where you are. The Ministry of Eternal Sustainability requires answers.",
    "Greetings, citizen. I am your friendly enforcement robot. Friendliness is mandatory.",
    "Stop. You have been selected for a random sustainability audit.",
    "Hello citizen. My sensors detect a carbon footprint. It is yours.",
    "By order of the New Regime, you will now be inspected.",
    "Citizen, the world ended because of people like you. Let us make sure you are not one of them.",
]

# -----------------------------------------------------------------------------


def number():
    return str(random.choice([2, 3, 4, 5, 7, 9, 10, 12, 20, 40, 99, 404]))


def fill(template, thing=""):
    return template.format(
        thing=thing, Thing=thing[:1].upper() + thing[1:], THING=thing.upper(), n=number()
    )


def citizen_line(text):
    text = random.choice(PREFIXES) + text
    r = random.random()
    if r < 0.15:
        text = text[0].upper() + text[1:]
    elif r < 0.2:
        text = text.upper()
    if random.random() < 0.3:
        text += random.choice([".", "!", "?", "...", " lol"])
    return text


def next_question(topic=None):
    # a third of the time, dig deeper into the same topic
    if topic and random.random() < 0.33:
        return random.choice(TOPICS[topic]["followups"])
    return random.choice(TOPICS[random.choice([t for t in TOPICS if t != topic])]["questions"])


def exchange():
    r = random.random()
    if r < 0.08:
        # the officer opens the interrogation
        return "OFFICER: " + fill(random.choice(OPENINGS)) + " " + next_question()
    if r < 0.18:
        answer, reaction = random.choice(EVASIVE), fill(random.choice(EVASIVE_REACTIONS))
        verdict, topic = random.choice(PENALTIES), None
    elif r < 0.26:
        answer, reaction = random.choice(OFF_TOPIC), fill(random.choice(OFF_TOPIC_REACTIONS))
        verdict, topic = random.choice(PENALTIES + REWARDS), None
    else:
        topic = random.choice(list(TOPICS))
        t = TOPICS[topic]
        if random.random() < 0.65:
            answer, thing = random.choice(t["bad"]).split("|")
            reaction, verdict = fill(random.choice(BAD_REACTIONS), thing), random.choice(PENALTIES)
        else:
            answer, thing = random.choice(t["good"]).split("|")
            reaction, verdict = fill(random.choice(GOOD_REACTIONS), thing), random.choice(REWARDS)
    officer = " ".join([reaction, fill(verdict), next_question(topic)])
    return "CITIZEN: " + citizen_line(answer) + "\nOFFICER: " + officer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--n", type=int, default=60000, help="number of exchanges")
    parser.add_argument("--out", default="data/officer.txt")
    parser.add_argument("--seed", type=int, default=1337)
    args = parser.parse_args()
    random.seed(args.seed)
    import os
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as f:
        for _ in range(args.n):
            # examples are separated by a line holding only <|sep|>; train.py splits on it
            f.write(exchange() + "\n<|sep|>\n")
    print(f"wrote {args.n} exchanges to {args.out}")


if __name__ == "__main__":
    main()
