# Win32 Word Guesser Game

## Overview

This is a multiplayer word guessing game implemented in C using Win32 APIs. Players connect to a central game server and compete to guess words using available letters. The project consists of four main components that work together to create a complete gaming experience.

## Architecture

The system follows a client-server architecture with shared memory for real-time communication:

### Components

1. **Overseer** (`Project1/overseer.c`) - Game Server
   - Central game controller and dictionary manager
   - Handles player connections via named pipes
   - Validates word guesses against dictionary
   - Manages game state and scoring system
   - Supports bot players with configurable reaction times

2. **JogoUI** (`JogoUI/jogoui.c`) - Console Game Client  
   - Text-based user interface for human players
   - Real-time letter display and game updates
   - Command system (`:pont` for score, `:jogs` for player list, `:sair` to quit)
   - Connects to overseer via named pipes

3. **Bot** (`Bot/bot.c`) - Automated Player
   - AI player that can be spawned by the overseer
   - Configurable reaction time (minimum 5000ms)
   - Automatically guesses words from available letters
   - Useful for testing and demonstration

4. **Painel** (`Painel/Painel.c`) - GUI Dashboard
   - Windows-based graphical interface
   - Real-time leaderboard display
   - Visual representation of available letters
   - Shows game statistics and last correct guess

## Features

### Core Gameplay
- **Letter-based word guessing**: Players form words using randomly generated letters
- **Dictionary validation**: All guesses checked against built-in dictionary (1236+ words)
- **Dynamic scoring**: Points awarded based on word length
- **Real-time updates**: Shared memory ensures instant game state synchronization

### Multiplayer Support
- **Named pipe communication**: Reliable client-server messaging
- **Player management**: Join, disconnect, kick functionality
- **Minimum 2 players**: Game automatically starts/pauses based on player count
- **Username uniqueness**: Prevents duplicate player names

### Bot Integration
- **Dynamic bot creation**: `bot create <username> <reaction_time_ms>`
- **Bot removal**: `bot remove <username>`
- **Configurable AI**: Adjustable reaction times for different difficulty levels

### Admin Controls (Overseer Commands)
- **Game rhythm**: `rythm up/down` - Adjust letter generation speed
- **Player management**: `kick <player>` - Remove disruptive players
- **Information display**: `show playerlist`, `show leaderboard`, `show mem`
- **Bot management**: Full bot lifecycle control

## System Requirements

- **Operating System**: Windows (Win32 API dependency)
- **Development Environment**: Visual Studio (solution file included)
- **Runtime**: Windows desktop environment for GUI components

## Build Instructions

1. Open `TrabalhoPratico_SistemasOperativos2.sln` in Visual Studio
2. Build the entire solution (Ctrl+Shift+B)
3. The following executables will be generated:
   - `overseer.exe` - Game server
   - `jogoui.exe` - Console client
   - `bot.exe` - Bot player
   - `painel.exe` - GUI dashboard

## Usage

### Starting a Game Session

1. **Launch the Overseer** (must be first):
   ```
   overseer.exe
   ```
   - Creates shared memory and named pipe server
   - Loads dictionary from `dictionary.txt`
   - Starts letter generation thread

2. **Connect Players**:
   ```
   jogoui.exe
   ```
   - Enter unique username when prompted
   - Game starts automatically when 2+ players connected

3. **Optional: Launch GUI Dashboard**:
   ```
   painel.exe
   ```
   - Shows real-time game statistics
   - Visual leaderboard and letter display

### Overseer Commands

```
?                    - Show help
kick <player>        - Remove a player
bot create <name> <time> - Add bot with reaction time (≥5000ms)
bot remove <name>    - Remove bot
rythm up/down        - Adjust game speed
show playerlist      - List connected players
show leaderboard     - Display scores
show mem             - Debug shared memory
exit                 - Shutdown server
```

### Player Commands (in JogoUI)

```
:pont               - Check your score
:jogs               - View player rankings
:sair               - Leave game
<word>              - Make a guess
```

## Technical Implementation

### Inter-Process Communication
- **Shared Memory**: Real-time game state sharing
- **Named Pipes**: Client-server message passing
- **Mutexes**: Thread-safe memory access
- **Events**: Synchronization signals

### Memory Management
- **GameSharedMem structure**: Central game state
- **Player lists**: Dynamic connection tracking
- **Letter arrays**: Available/displayed letter management

### Threading Model
- **Letter generation thread**: Periodic letter updates
- **Client handler threads**: One per connected player
- **Broadcast thread**: Real-time message distribution
- **Validation thread**: Word checking and scoring

## Configuration

The system uses Windows Registry for persistent settings:
- **MAXLETRAS**: Number of available letters (default: 6)
- **RITMO**: Letter generation interval in milliseconds (default: 3000)

## File Structure

```
├── Project1/                 # Overseer (server)
│   ├── overseer.c
│   ├── utils.h
│   └── Project1.vcxproj
├── JogoUI/                   # Console client
│   ├── jogoui.c
│   ├── utils.h
│   └── JogoUI.vcxproj
├── Bot/                      # Automated player
│   ├── bot.c
│   └── Bot.vcxproj
├── Painel/                   # GUI dashboard
│   ├── Painel.c
│   ├── Painel.h
│   ├── resources/
│   └── Painel.vcxproj
└── TrabalhoPratico_SistemasOperativos2.sln
```

## Development Notes

This project demonstrates advanced Win32 programming concepts:
- Multi-process architecture with IPC
- Thread synchronization and shared memory
- Named pipe server/client implementation
- Windows GUI programming with resource management
- Real-time game state management

The code includes extensive Portuguese comments and demonstrates practical systems programming for multiplayer game development on Windows platforms.
