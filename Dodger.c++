#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm> // For std::max, std::min, std::transform
#include <map>       // For DIFFICULTY_SETTINGS and WIN_THRESHOLD_TIMES
#include <fstream>   // For file input/output
#include <sstream>   // For string stream operations
#include <cmath>     // For std::abs, cosf, sinf, atan2f (float versions), fmaxf, fminf
#include <limits.h>  // For PATH_MAX (Standard C header for limits)
#include <unistd.h>  // For readlink (Linux-specific)
#include <set>       // For std::set to get unique usernames

// --- Game Constants (Global or passed around) ---
// Changed to non-const so they can be updated on window resize/fullscreen toggle
int SCREEN_WIDTH = 1366;
int SCREEN_HEIGHT = 694; // Match the original JS canvas dimensions

// Colors (Raylib has predefined ones, we'll use those to avoid redefinition errors)
const Color LIGHTGRAY_CUSTOM = {200, 200, 200, 255}; // A lighter gray for text
const Color PROJECTILE_COLOR = YELLOW; // Player projectile color
const Color OBSTACLE_STUNNED_COLOR = GRAY; // New color for stunned obstacle
const Color OBSTACLE_PROJECTILE_COLOR = MAGENTA; // New color for obstacle's projectiles
const Color UNLOCKED_ACHIEVEMENT_COLOR = GOLD;
const Color LOCKED_ACHIEVEMENT_COLOR = DARKGRAY;
const Color SELECTED_ITEM_COLOR = SKYBLUE; // For menu selection

// --- Game State Enums (for clarity) ---
enum GameState {
    GAME_STATE_USERNAME_INPUT = 0, // State for username input
    GAME_STATE_MAIN_MENU,          // New state: After username, choose Play or Achievements
    GAME_STATE_COUNTDOWN,
    GAME_STATE_PLAYING,
    GAME_STATE_GAME_OVER,
    GAME_STATE_ACHIEVEMENTS,       // Achievements display screen
    GAME_STATE_SELECT_ACHIEVEMENT_PROFILE, // New state: For assigning Portal achievement
    GAME_STATE_TAMPERED            // New state for detected tampering
};

// --- Difficulty Settings Structure ---
struct DifficultySettings {
    float player_speed;
    float obstacle_speed;
    int ai_reaction_delay; // In frames
};

// Enum for text alignment
enum TextAlignment {
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

// --- Game Elements: Projectile Struct ---
struct Projectile {
    Rectangle rect; // x, y, width, height
    float speed;
    Vector2 velocity; // vx, vy
    bool active;
    bool is_player_shot; // True if shot by player, false if by obstacle
    int bounces_remaining; // How many bounces left for the projectile before it starts wrapping
};

// --- Global Game Variables ---
// Gameplay related
Color obstacle_color = RED; // Using Raylib's predefined RED
float obstacle_x;
float obstacle_y;
float obstacle_size = 50.0f;
float obstacle_speed; // Will be set by difficulty

Color player_color = GREEN; // Using Raylib's predefined GREEN
float player_x;
float player_y;
float player_size = 50.0f;
float player_speed; // Will be set by difficulty

float player_vx = 0.0f; // Player velocity X (for normal movement)
float player_vy = 0.0f; // Player velocity Y (for normal movement)
float player_aim_angle = 0.0f; // New: Angle for player's aiming direction (in radians)

float ai_target_x;
float ai_target_y;
int ai_reaction_timer = 0;
int ai_reaction_delay; // Will be set by difficulty
const int prediction_frames = 30;

GameState current_game_state = GAME_STATE_USERNAME_INPUT; // Start with username input

// Countdown
const int FPS = 60; // Frames per second, used for countdown timing
const int countdown_time_seconds = 3;
int countdown_timer_frames = countdown_time_seconds * FPS;
int current_countdown_frame;

// Time tracking
double start_time_s = 0.0; // Seconds since game start
double elapsed_time_s = 0.0; // Total survival time in seconds
double final_survival_time_s = 0.0;

// High Score (Now persistent via file I/O)
// Note: high_scores now stores only the current user's high score for the current difficulty.
// Full high score data for all users/difficulties would require a more complex structure.
std::map<std::string, double> high_scores; // Maps difficulty_mode to high score for current_username
bool is_new_high_score = false; // Correct variable name

// Difficulty Management
std::map<std::string, DifficultySettings> DIFFICULTY_SETTINGS = {
    {"normal",       {5.0f,  6.0f,  60}} // Player speed 5.0f, OBSTACLE SPEED INCREASED to 6.0f, AI delay 60
};

std::vector<std::string> DIFFICULTIES_ORDER = {"normal"}; // Only normal difficulty now

std::map<std::string, double> WIN_THRESHOLD_TIMES = {
    {"normal",       60.0}  // WIN THRESHOLD CHANGED to 60 seconds (1 minute) for normal
};

std::string current_difficulty_mode = "normal"; // Game always starts on normal mode

// Username related (Now persistent)
std::string current_username = "Guest"; // Default username
std::string username_input_buffer = ""; // Buffer for collecting typed characters
const int MAX_USERNAME_LENGTH = 15;
std::string last_active_username = "Guest"; // Stores the last username that was active

// File persistence
const std::string SAVE_FILE_NAME = "dodger_data.json";

// Audio variables
Music normal_music; // Renamed for normal game soundtrack (oiaa_oiaa.mp3)
Music win_music;    // Renamed for win soundtrack (rat_dance_audio_only.mp3)

// --- Projectile Variables ---
std::vector<Projectile> projectiles;
const float PROJECTILE_SPEED = 10.0f;
const float PROJECTILE_SIZE = 10.0f;
const float PLAYER_SHOOT_COOLDOWN = 0.5; // seconds for normal projectile
double player_last_shot_time = -PLAYER_SHOOT_COOLDOWN; // Initialize to allow immediate first shot
const int MAX_PROJECTILE_BOUNCES = 5; // Projectiles bounce 5 times, then wrap

// --- Stun Mechanic Variables ---
bool obstacle_is_stunned = false;
double obstacle_stun_end_time = 0.0;
const double OBSTACLE_STUN_DURATION = 3.0; // seconds obstacle is stunned
const double PLAYER_STUN_SHOT_COOLDOWN = 5.0; // seconds cooldown for player's stun shot
double player_last_stun_shot_time = -PLAYER_STUN_SHOT_COOLDOWN; // Initialize to allow immediate first stun shot

// Player stun variables
bool player_is_stunned = false;
double player_stun_end_time = 0.0;
const double PLAYER_STUN_DURATION = 2.0; // seconds player is stunned
const double OBSTACLE_STUN_COOLDOWN = 5.0; // Cooldown for obstacle to stun player
double obstacle_last_stun_time = -OBSTACLE_STUN_COOLDOWN; // Initialize to allow immediate first stun

// --- Obstacle Shooting Variables ---
const float OBSTACLE_PROJECTILE_SPEED = 8.0f; // Slower than player's
const float OBSTACLE_SHOOT_COOLDOWN = 2.0; // Obstacle shoots every 2 seconds (re-introduced cooldown)
double obstacle_last_shot_time = -OBSTACLE_SHOOT_COOLDOWN; // Initialize to allow immediate first shot

// --- Player Dash Variables ---
double player_last_dash_time = -2.0; // Initialize to allow immediate first dash (PLAYER_DASH_COOLDOWN)
const double PLAYER_DASH_COOLDOWN = 2.0; // seconds
const float PLAYER_DASH_DISTANCE = 150.0f; // Total distance of the dash
const double PLAYER_DASH_DURATION = 0.15; // seconds (duration of the dash, for invincibility and movement)
bool player_is_dashing = false;
double player_dash_end_time = 0.0;
float player_dash_velocity_x = 0.0f; // New: Velocity for dash movement
float player_dash_velocity_y = 0.0f; // New: Velocity for dash movement

// --- Anti-Tampering Variables ---
// IMPORTANT: This checksum is for a specific, compiled executable.
// If you modify the source code (Dodger.c++) and recompile, the checksum of the
// NEW EXECUTABLE will change. This means this EXPECTED_CHECKSUM will no longer match.
//
// This type of simple client-side checksum cannot differentiate between legitimate
// developer changes and malicious user tampering if the checksum value itself
// is embedded within the file being checked. Changing the checksum value in source
// changes the source, which changes the compiled executable, leading to a new checksum.
//
// For development, it's highly recommended to keep the anti-tampering check commented out.
// If you want to enable it for a specific "release" binary, you would:
// 1. Make all final code changes to Dodger.c++.
// 2. Compile it once: `g++ Dodger.c++ -o Dodger-game -lraylib`
// 3. Run the Python script on the NEWLY COMPILED EXECUTABLE to get its checksum:
//    `python calculate_checksum.py Dodger-game`
// 4. Copy the output checksum.
// 5. UNCOMMENT THE ANTI-TAMPERING BLOCK BELOW and update this EXPECTED_CHECKSUM value
//    in Dodger.c++ with the new checksum.
// 6. Compile Dodger.c++ ONE MORE TIME: `g++ Dodger.c++ -o Dodger-game -lraylib`
// This final compilation embeds the correct checksum for that specific binary.
// Any external modification to *that specific binary* will then trigger the message.
const long long EXPECTED_CHECKSUM = 1234567890; // Placeholder - will need to be updated for a release build

// Global variable for game over message
std::string nextGameMessage = ""; // Moved to global scope

// --- Time Bonus Variables ---
double dodge_streak_start_time = 0.0; // Time when the current dodge streak began
const double DODGE_BONUS_INTERVAL = 10.0; // Every 10 seconds of continuous dodging
const double DODGE_BONUS_AMOUNT = 1.0; // Add 1 second to time
bool showing_time_bonus_message = false;
double time_bonus_message_end_time = 0.0;
const double TIME_BONUS_MESSAGE_DURATION = 1.0; // Duration for the "TIME BONUS" message

// --- Achievement System Variables ---
struct Achievement {
    std::string id;
    std::string name;
    std::string description;
    bool is_secret; // New: true if achievement should be hidden until unlocked
    // Constructor for easier initialization
    Achievement(std::string id, std::string name, std::string description, bool is_secret = false)
        : id(std::move(id)), name(std::move(name)), description(std::move(description)), is_secret(is_secret) {}
};

// Global map of all possible achievements
std::map<std::string, Achievement> ALL_ACHIEVEMENTS;

// Global map to store which achievements are unlocked for each user
// Key: username, Value: vector of unlocked achievement IDs
std::map<std::string, std::vector<std::string>> UNLOCKED_ACHIEVEMENTS_BY_USER;

// Game-specific flags for achievement tracking (reset per game)
bool near_miss_achievement_unlocked_this_game = false;
int obstacle_stuns_this_game = 0;
bool has_shot_this_game = false; // To track if player has shot for "Bullet Ballet Master"
bool dash_through_projectile_achievement_unlocked_this_game = false; // For "Dash of Genius"

// Achievement popup display variables
std::string current_achievement_popup_id = "";
double achievement_popup_display_end_time = 0.0;
const double ACHIEVEMENT_POPUP_DURATION = 3.0; // seconds the popup is displayed

// --- Portal Mode Variables ---
bool is_portal_mode = false; // True if current_username is "PORTAL"
bool portal_1_active = false;
Vector2 portal_1_pos = {0,0};
bool portal_2_active = false;
Vector2 portal_2_pos = {0,0};
const float PORTAL_RADIUS = 30.0f;
const Color PORTAL_COLOR_1 = BLUE;
const Color PORTAL_COLOR_2 = ORANGE;
double last_teleport_time = 0.0;
const double TELEPORT_COOLDOWN = 0.5; // seconds to prevent rapid teleporting

// New variables for projectile-created portals
double portal_active_until_time_1 = 0.0;
double portal_active_until_time_2 = 0.0;
const double PORTAL_ACTIVE_DURATION = 10.0; // seconds portals stay active
bool next_projectile_portal_is_1 = true; // To toggle between placing portal 1 and 2

// --- Achievement Profile Selection Variables ---
std::vector<std::string> available_profile_names; // List of usernames to choose from
int selected_profile_index = 0; // Index of the currently selected profile

// Helper function to unlock an achievement
void unlockAchievement(const std::string& achievementId, const std::string& targetUsername); // Prototype added here

// Function to initialize all achievement definitions
void initializeAchievementDefinitions(); // Prototype added here

// --- Function Declarations (Prototypes) ---
void resetGame();
void applyDifficulty(const std::string& mode);
void updateGame(double deltaTime);
void drawGame();
void drawCenteredText(const std::string& text, int fontSize, Color color, int yOffset = 0);
void drawInfoText(const std::string& text, int fontSize, Color color, int x, int y, TextAlignment align);
void drawAchievementsScreen(); // New function for achievements screen
void drawSelectAchievementProfileScreen(); // New function for profile selection

// Persistence functions
void saveGameData(); // Prototype added here
void loadGameData(); // Prototype added here
void initializeDefaultGameData(); // Prototype added here

// Anti-tampering functions
std::string getExecutablePath();
long long calculateFileChecksum(const std::string& filePath);

// --- Main Function ---
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Dodger Game - Raylib C++");
    
    // Initialize SCREEN_WIDTH and SCREEN_HEIGHT with actual window dimensions
    SCREEN_WIDTH = GetScreenWidth();
    SCREEN_HEIGHT = GetScreenHeight();

    SetTargetFPS(FPS);
    SetExitKey(KEY_NULL); // Disable default ESC key behavior for closing window

    // --- Anti-Tampering Check (COMMENTED OUT FOR DEVELOPMENT EASE) ---
    /*
    std::string exePath = getExecutablePath();
    if (!exePath.empty()) {
        long long currentExecutableChecksum = calculateFileChecksum(exePath);
        TraceLog(LOG_INFO, "Executable path: %s", exePath.c_str());
        TraceLog(LOG_INFO, "Calculated executable checksum: %lld", currentExecutableChecksum);
        TraceLog(LOG_INFO, "Expected executable checksum: %lld", EXPECTED_CHECKSUM);

        if (currentExecutableChecksum != EXPECTED_CHECKSUM) {
            TraceLog(LOG_WARNING, "Executable tampering detected! Checksum mismatch.");
            current_game_state = GAME_STATE_TAMPERED;
        }
    } else {
        TraceLog(LOG_ERROR, "Could not determine executable path. Anti-tampering check skipped.");
        current_game_state = GAME_STATE_TAMPERED;
    }
    */
    // --- End Anti-Tampering Check ---

    // --- Audio Initialization ---
    InitAudioDevice();

    normal_music = LoadMusicStream("oiaa_oiaa.mp3");
    if (normal_music.frameCount > 0) {
        TraceLog(LOG_INFO, "Music 'oiaa_oiaa.mp3' loaded successfully for normal game.");
        SetMusicVolume(normal_music, 0.5f);
    } else {
        TraceLog(LOG_WARNING, "Failed to load music 'oiaa_oiaa.mp3'. Make sure the file exists and is a valid MP3.");
    }

    win_music = LoadMusicStream("rat_dance_audio_only.mp3");
    if (win_music.frameCount > 0) {
        TraceLog(LOG_INFO, "Music 'rat_dance_audio_only.mp3' loaded successfully for win screen.");
        SetMusicVolume(win_music, 0.5f);
    } else {
        TraceLog(LOG_WARNING, "Failed to load music 'rat_dance_audio_only.mp3'. Make sure the file exists and is a valid MP3.");
    }

    if (normal_music.frameCount > 0) {
        PlayMusicStream(normal_music);
    }
    // --- End Audio Initialization ---

    initializeAchievementDefinitions(); // Initialize all achievement data
    loadGameData();
    username_input_buffer = current_username; // Set input buffer to current username on start

    // Game Loop
    while (!WindowShouldClose()) { // WindowShouldClose() will now only be true if CloseWindow() is called manually
        double deltaTime = GetFrameTime();

        // Check for fullscreen toggle (F key) or window resize and update dimensions
        if (IsKeyPressed(KEY_F)) {
            ToggleFullscreen();
            SCREEN_WIDTH = GetScreenWidth();
            SCREEN_HEIGHT = GetScreenHeight();
            // Re-center player and obstacle if they were off-screen or in awkward positions
            player_x = (float)SCREEN_WIDTH / 2.0f - player_size / 2.0f;
            player_y = (float)SCREEN_HEIGHT - player_size;
            obstacle_x = (float)SCREEN_WIDTH / 2.0f - obstacle_size / 2.0f;
            obstacle_y = 0.0f;
        } else if (IsWindowResized()) {
            SCREEN_WIDTH = GetScreenWidth();
            SCREEN_HEIGHT = GetScreenHeight();
        }

        if (current_game_state != GAME_STATE_TAMPERED) {
            if (normal_music.frameCount > 0 && IsMusicStreamPlaying(normal_music)) {
                UpdateMusicStream(normal_music);
            }
            if (win_music.frameCount > 0 && IsMusicStreamPlaying(win_music)) {
                UpdateMusicStream(win_music);
            }
        }

        updateGame(deltaTime);

        BeginDrawing();
        ClearBackground(BLACK);

        drawGame();

        EndDrawing();

        // The only place CloseWindow() should be called directly for quitting
        if (current_game_state == GAME_STATE_TAMPERED) {
            CloseWindow();
        }
    }

    // --- Audio De-Initialization ---
    if (normal_music.frameCount > 0) {
        UnloadMusicStream(normal_music);
    }
    if (win_music.frameCount > 0) {
        UnloadMusicStream(win_music);
    }
    CloseAudioDevice();
    // --- End Audio Initialization ---

    CloseWindow();
    return 0;
}

// --- Function Definitions (Implementations) ---

std::string getExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1) {
        return std::string(result, count);
    }
    return "";
}

long long calculateFileChecksum(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "Failed to open file for checksum: %s. It might be missing or tampered with.", filePath.c_str());
        return 0;
    }

    long long checksum = 0;
    char byte;
    while (file.get(byte)) {
        checksum += static_cast<unsigned char>(byte);
    }
    file.close();
    return checksum;
}


void initializeDefaultGameData() {
    last_active_username = "Guest";
    current_username = "Guest"; // Ensure current_username is set to default
    high_scores["normal"] = 0.0;
    UNLOCKED_ACHIEVEMENTS_BY_USER.clear(); // Clear any existing achievement data
    UNLOCKED_ACHIEVEMENTS_BY_USER[current_username] = {}; // Initialize empty for default user
    TraceLog(LOG_INFO, "Initialized default game data.");
    saveGameData();
}

void saveGameData() {
    std::ofstream outFile(SAVE_FILE_NAME);
    if (outFile.is_open()) {
        outFile << "{\n";
        outFile << "  \"last_username\": \"" << current_username << "\",\n"; // Save the last active username
        outFile << "  \"high_scores\": {\n";
        outFile << "    \"normal\": " << high_scores["normal"]; // Only saving current user's normal high score
        outFile << "\n  },\n";

        outFile << "  \"user_data\": {\n";
        bool first_user_entry = true;
        for (const auto& user_entry : UNLOCKED_ACHIEVEMENTS_BY_USER) {
            if (!first_user_entry) {
                outFile << ",\n";
            }
            outFile << "    \"" << user_entry.first << "\": {\n";
            outFile << "      \"unlocked_achievements\": [";
            bool first_achievement = true;
            for (const auto& achievement_id : user_entry.second) {
                if (!first_achievement) {
                    outFile << ", ";
                }
                outFile << "\"" << achievement_id << "\"";
                first_achievement = false;
            }
            outFile << "]\n";
            outFile << "    }"; // Close user_data object
            first_user_entry = false;
        }
        outFile << "\n  }\n"; // Close user_data
        outFile << "}\n"; // Close main JSON object
        outFile.close();
        TraceLog(LOG_INFO, "Game data saved successfully to %s.", SAVE_FILE_NAME.c_str());
    } else {
        TraceLog(LOG_WARNING, "Could not open file %s for saving game data.", SAVE_FILE_NAME.c_str());
    }
}

void loadGameData() {
    std::ifstream inFile(SAVE_FILE_NAME);
    if (inFile.is_open()) {
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        std::string json_str = buffer.str();
        inFile.close();

        TraceLog(LOG_INFO, "Loaded game data from %s:\n%s", SAVE_FILE_NAME.c_str(), json_str.c_str());

        // Parse last_username
        size_t last_username_pos = json_str.find("\"last_username\": \"");
        if (last_username_pos != std::string::npos) {
            last_username_pos += std::string("\"last_username\": \"").length();
            size_t last_username_end = json_str.find("\"", last_username_pos);
            if (last_username_end != std::string::npos) {
                last_active_username = json_str.substr(last_username_pos, last_username_end - last_username_pos);
                current_username = last_active_username; // Set current_username to the last active one
            }
        }

        // Parse high scores (only for the current_username, as before)
        size_t scores_start = json_str.find("\"high_scores\": {");
        if (scores_start != std::string::npos) {
            scores_start += std::string("\"high_scores\": {").length();
            size_t scores_end = json_str.find("}", scores_start);
            if (scores_end != std::string::npos) {
                std::string scores_content = json_str.substr(scores_start, scores_end - scores_start);
                size_t key_start = scores_content.find("\"normal\":");
                if (key_start != std::string::npos) {
                    key_start += std::string("\"normal\":").length();
                    try {
                        high_scores["normal"] = std::stod(scores_content.substr(key_start));
                    } catch (const std::invalid_argument& e) {
                        TraceLog(LOG_WARNING, "Invalid number format in save file for 'normal' high score.");
                    } catch (const std::out_of_range& e) {
                        TraceLog(LOG_WARNING, "Number out of range in save file for 'normal' high score.");
                    }
                } else {
                    high_scores["normal"] = 0.0;
                }
            }
        }

        // Parse all unlocked achievements for all users
        UNLOCKED_ACHIEVEMENTS_BY_USER.clear(); // Clear before loading
        size_t user_data_start = json_str.find("\"user_data\": {");
        if (user_data_start != std::string::npos) {
            user_data_start += std::string("\"user_data\": {").length();
            size_t user_data_end = json_str.find("}", user_data_start);
            if (user_data_end == std::string::npos) { // Handle case where user_data is empty or malformed
                user_data_end = json_str.length(); // Read to end of file if no closing brace
            }
            std::string user_data_content = json_str.substr(user_data_start, user_data_end - user_data_start);
            
            // Simple parsing: find usernames and their achievement arrays
            size_t current_pos = 0;
            while ((current_pos = user_data_content.find("\"", current_pos)) != std::string::npos) {
                size_t user_name_start = current_pos + 1;
                size_t user_name_end = user_data_content.find("\"", user_name_start);
                if (user_name_end == std::string::npos) break;
                std::string username_key = user_data_content.substr(user_name_start, user_name_end - user_name_start);
                
                size_t achievements_array_start = user_data_content.find("[", user_name_end);
                if (achievements_array_start == std::string::npos) break;
                size_t achievements_array_end = user_data_content.find("]", achievements_array_start);
                if (achievements_array_end == std::string::npos) break;

                std::string achievement_list_str = user_data_content.substr(achievements_array_start + 1, achievements_array_end - achievements_array_start - 1);
                std::stringstream list_ss(achievement_list_str);
                std::string achievement_id;
                while (std::getline(list_ss, achievement_id, ',')) {
                    // Trim whitespace and quotes
                    size_t first = achievement_id.find_first_not_of(" \"\n\r\t");
                    size_t last = achievement_id.find_last_not_of(" \"\n\r\t");
                    if (std::string::npos == first) continue;
                    UNLOCKED_ACHIEVEMENTS_BY_USER[username_key].push_back(achievement_id.substr(first, (last - first + 1)));
                }
                current_pos = achievements_array_end + 1;
            }
        }

        // If no user data was loaded, ensure the current_username has an empty achievement list
        if (UNLOCKED_ACHIEVEMENTS_BY_USER.find(current_username) == UNLOCKED_ACHIEVEMENTS_BY_USER.end()) {
            UNLOCKED_ACHIEVEMENTS_BY_USER[current_username] = {};
        }

        TraceLog(LOG_INFO, "Game data loaded successfully. Last active username: %s", current_username.c_str());
    } else {
        TraceLog(LOG_INFO, "Save file %s not found or could not be opened. Initializing default data.", SAVE_FILE_NAME.c_str());
        initializeDefaultGameData();
    }
}

// Helper function to unlock an achievement for a specific user
void unlockAchievement(const std::string& achievementId, const std::string& targetUsername) {
    // Check if the achievement exists in ALL_ACHIEVEMENTS
    if (ALL_ACHIEVEMENTS.count(achievementId) == 0) {
        TraceLog(LOG_WARNING, "Attempted to unlock non-existent achievement: %s", achievementId.c_str());
        return;
    }

    // Ensure the target user's entry exists in UNLOCKED_ACHIEVEMENTS_BY_USER
    if (UNLOCKED_ACHIEVEMENTS_BY_USER.find(targetUsername) == UNLOCKED_ACHIEVEMENTS_BY_USER.end()) {
        UNLOCKED_ACHIEVEMENTS_BY_USER[targetUsername] = {}; // Create an empty vector for the user
    }

    // Check if the achievement is already unlocked for the target user
    bool already_unlocked = false;
    for (const auto& id : UNLOCKED_ACHIEVEMENTS_BY_USER.at(targetUsername)) {
        if (id == achievementId) {
            already_unlocked = true;
            break;
        }
    }

    if (!already_unlocked) {
        UNLOCKED_ACHIEVEMENTS_BY_USER.at(targetUsername).push_back(achievementId);
        TraceLog(LOG_INFO, "Achievement Unlocked for %s: %s", targetUsername.c_str(), ALL_ACHIEVEMENTS.at(achievementId).name.c_str());
        saveGameData(); // Save immediately when an achievement is unlocked

        // Only show popup if the achievement was unlocked for the currently active user
        if (targetUsername == current_username) {
            current_achievement_popup_id = achievementId;
            achievement_popup_display_end_time = GetTime() + ACHIEVEMENT_POPUP_DURATION;
        }
    }
}

// Function to initialize all achievement definitions
void initializeAchievementDefinitions() {
    ALL_ACHIEVEMENTS.clear(); // Clear any previous definitions

    ALL_ACHIEVEMENTS.emplace("portal_username", Achievement(
        "portal_username",
        "vrooom wait that's too *whoosh*",
        "Set your username to 'PORTAL'.",
        true // This is a secret achievement
    ));
    ALL_ACHIEVEMENTS.emplace("near_miss", Achievement(
        "near_miss",
        "*whew* that was a close call",
        "Perform a truly close dodge against a projectile."
    ));
    ALL_ACHIEVEMENTS.emplace("bullet_ballet_master", Achievement(
        "bullet_ballet_master",
        "Bullet Ballet Master",
        "Survive for 30 seconds without firing a single shot."
    ));
    ALL_ACHIEVEMENTS.emplace("stunned_silence", Achievement(
        "stunned_silence",
        "Stunned Silence",
        "Stun the enemy obstacle 3 times in one game."
    ));
    ALL_ACHIEVEMENTS.emplace("dash_of_genius", Achievement(
        "dash_of_genius",
        "Dash of Genius",
        "Successfully dash through an enemy projectile."
    ));
    ALL_ACHIEVEMENTS.emplace("long_haul_dodger", Achievement(
        "long_haul_dodger",
        "Long-Haul Dodger",
        "Survive for 120 seconds (2 minutes)."
    ));
    // Add more achievements here!
}


void resetGame() {
    player_x = (float)SCREEN_WIDTH / 2.0f - player_size / 2.0f;
    player_y = (float)SCREEN_HEIGHT - player_size;
    
    obstacle_x = (float)SCREEN_WIDTH / 2.0f - obstacle_size / 2.0f;
    obstacle_y = 0.0f;

    player_vx = 0.0f;
    player_vy = 0.0f;
    player_aim_angle = 0.0f;
    ai_reaction_timer = 0;

    start_time_s = 0.0;
    elapsed_time_s = 0.0;
    final_survival_time_s = 0.0;
    is_new_high_score = false;

    current_countdown_frame = countdown_time_seconds * FPS;
    // Game state is set to COUNTDOWN from MAIN_MENU, not here
    applyDifficulty("normal");

    if (win_music.frameCount > 0 && IsMusicStreamPlaying(win_music)) {
        StopMusicStream(win_music);
    }
    if (normal_music.frameCount > 0 && !IsMusicStreamPlaying(normal_music)) {
        PlayMusicStream(normal_music);
    }

    projectiles.clear();
    player_last_shot_time = -PLAYER_SHOOT_COOLDOWN;

    obstacle_is_stunned = false;
    obstacle_stun_end_time = 0.0;
    player_last_stun_shot_time = -PLAYER_STUN_SHOT_COOLDOWN;
    obstacle_last_shot_time = -OBSTACLE_SHOOT_COOLDOWN;

    player_is_dashing = false; // Reset dash state on game reset
    player_dash_end_time = 0.0;
    player_last_dash_time = -PLAYER_DASH_COOLDOWN; // Reset dash cooldown
    player_dash_velocity_x = 0.0f; // Reset dash velocity
    player_dash_velocity_y = 0.0f; // Reset dash velocity

    player_is_stunned = false;
    player_stun_end_time = 0.0;
    obstacle_last_stun_time = -OBSTACLE_STUN_COOLDOWN;

    // Reset dodge streak timer
    dodge_streak_start_time = 0.0;
    showing_time_bonus_message = false;
    time_bonus_message_end_time = 0.0;

    // Reset achievement tracking flags for new game
    near_miss_achievement_unlocked_this_game = false;
    obstacle_stuns_this_game = 0;
    has_shot_this_game = false;
    dash_through_projectile_achievement_unlocked_this_game = false;
    current_achievement_popup_id = ""; // Clear any pending popup
    achievement_popup_display_end_time = 0.0;

    // Reset portal mode variables
    portal_1_active = false;
    portal_2_active = false;
    last_teleport_time = 0.0;
    portal_active_until_time_1 = 0.0;
    portal_active_until_time_2 = 0.0;
    next_projectile_portal_is_1 = true;
    // is_portal_mode is set based on username, not reset here
}

void applyDifficulty(const std::string& mode) {
    current_difficulty_mode = "normal";
    const DifficultySettings& settings = DIFFICULTY_SETTINGS["normal"];
    player_speed = settings.player_speed;
    obstacle_speed = settings.obstacle_speed;
    ai_reaction_delay = settings.ai_reaction_delay;
    TraceLog(LOG_INFO, "Difficulty forced to: normal (Player Speed: %.1f, Obstacle Speed: %.1f)", player_speed, obstacle_speed);
}

void updateGame(double deltaTime) {
    if (current_game_state == GAME_STATE_TAMPERED) {
        return;
    }

    if (current_game_state == GAME_STATE_USERNAME_INPUT) {
        for (int key = KEY_A; key <= KEY_Z; ++key) {
            if (IsKeyPressed(key) && username_input_buffer.length() < MAX_USERNAME_LENGTH) {
                username_input_buffer += (char)key;
            }
        }
        for (int key = KEY_ZERO; key <= KEY_NINE; ++key) {
            if (IsKeyPressed(key) && username_input_buffer.length() < MAX_USERNAME_LENGTH) {
                username_input_buffer += (char)key;
            }
        }
        if (IsKeyPressed(KEY_SPACE) && username_input_buffer.length() < MAX_USERNAME_LENGTH) {
            username_input_buffer += ' ';
        }
        if (IsKeyPressed(KEY_MINUS) && username_input_buffer.length() < MAX_USERNAME_LENGTH) {
            username_input_buffer += '-';
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (username_input_buffer.length() > 0) {
                username_input_buffer.pop_back();
            }
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (username_input_buffer.empty()) {
                current_username = "Guest";
            } else {
                current_username = username_input_buffer;
            }
            // If "PORTAL" username is entered, go to achievement profile selection
            if (current_username == "PORTAL") {
                is_portal_mode = true; // Enable portal mode immediately for gameplay
                TraceLog(LOG_INFO, "Portal mode enabled!");
                
                // Populate available_profile_names for selection
                available_profile_names.clear();
                for (const auto& pair : UNLOCKED_ACHIEVEMENTS_BY_USER) {
                    available_profile_names.push_back(pair.first);
                }
                // Add current_username if it's not already in the list (e.g., brand new profile)
                if (std::find(available_profile_names.begin(), available_profile_names.end(), current_username) == available_profile_names.end()) {
                    available_profile_names.push_back(current_username);
                }
                // Sort for consistent display
                std::sort(available_profile_names.begin(), available_profile_names.end());
                
                // Set selected_profile_index to the current_username if possible
                selected_profile_index = 0;
                for(size_t i = 0; i < available_profile_names.size(); ++i) {
                    if (available_profile_names[i] == last_active_username) {
                        selected_profile_index = i;
                        break;
                    }
                }

                current_game_state = GAME_STATE_SELECT_ACHIEVEMENT_PROFILE;
            } else {
                is_portal_mode = false; // Disable portal mode for other usernames
                last_active_username = current_username; // Update last active username
                saveGameData(); // Save the new current_username as last active
                username_input_buffer = "";
                current_game_state = GAME_STATE_MAIN_MENU; // Go to main menu
            }
        }

        // Only ESC to close window from username input screen
        if (IsKeyPressed(KEY_ESCAPE)) {
            CloseWindow(); // This will make WindowShouldClose() return true and exit the main loop
        }

    } else if (current_game_state == GAME_STATE_MAIN_MENU) {
        if (IsKeyPressed(KEY_ENTER)) {
            resetGame(); // Reset game state before starting
            current_game_state = GAME_STATE_COUNTDOWN; // Start playing
        }
        if (IsKeyPressed(KEY_A)) {
            current_game_state = GAME_STATE_ACHIEVEMENTS;
        }
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) { // P OR ESC to change profile
            current_game_state = GAME_STATE_USERNAME_INPUT; // Go back to username input
            username_input_buffer = current_username; // Pre-fill input with current username
        }

    } else if (current_game_state == GAME_STATE_SELECT_ACHIEVEMENT_PROFILE) {
        if (IsKeyPressed(KEY_UP)) {
            selected_profile_index = (selected_profile_index - 1 + available_profile_names.size()) % available_profile_names.size();
        }
        if (IsKeyPressed(KEY_DOWN)) {
            selected_profile_index = (selected_profile_index + 1) % available_profile_names.size();
        }
        if (IsKeyPressed(KEY_ENTER)) {
            std::string chosen_profile = available_profile_names[selected_profile_index];
            unlockAchievement("portal_username", chosen_profile); // Unlock for chosen profile
            
            // After selection, go back to main menu to allow playing with "PORTAL"
            current_username = "PORTAL"; // Keep username as PORTAL for gameplay
            last_active_username = current_username; // Update last active username
            saveGameData(); // Save the new last active username and achievement
            username_input_buffer = ""; // Clear buffer for next input
            current_game_state = GAME_STATE_MAIN_MENU; // Go to main menu after selection
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            current_game_state = GAME_STATE_USERNAME_INPUT; // Go back to username input
            username_input_buffer = current_username; // Pre-fill input with current username
        }

    } else if (current_game_state == GAME_STATE_COUNTDOWN) {
        current_countdown_frame--;
        if (current_countdown_frame <= 0) {
            current_game_state = GAME_STATE_PLAYING;
            start_time_s = GetTime();
            dodge_streak_start_time = GetTime(); // Initialize dodge streak timer
            has_shot_this_game = false; // Reset for "Bullet Ballet Master"
            obstacle_stuns_this_game = 0; // Reset for "Stunned Silence"
            dash_through_projectile_achievement_unlocked_this_game = false; // Reset for "Dash of Genius"

            if (normal_music.frameCount > 0 && !IsMusicStreamPlaying(normal_music)) {
                PlayMusicStream(normal_music);
            }
            if (win_music.frameCount > 0 && IsMusicStreamPlaying(win_music)) {
                StopMusicStream(win_music);
            }
        }
    } else if (current_game_state == GAME_STATE_PLAYING) {
        elapsed_time_s = GetTime() - start_time_s;

        // "Bullet Ballet Master" achievement check
        // Check if player has survived for 30 seconds AND has not shot
        if (!has_shot_this_game && elapsed_time_s >= 30.0) {
            unlockAchievement("bullet_ballet_master", current_username);
        }
        // "Long-Haul Dodger" achievement check
        if (elapsed_time_s >= 120.0) { // 2 minutes
            unlockAchievement("long_haul_dodger", current_username);
        }


        // Handle dash activation
        if ((IsKeyPressed(KEY_LEFT_ALT) || IsKeyPressed(KEY_RIGHT_ALT)) && GetTime() - player_last_dash_time >= PLAYER_DASH_COOLDOWN) {
            player_is_dashing = true;
            player_dash_end_time = GetTime() + PLAYER_DASH_DURATION;
            player_last_dash_time = GetTime();

            float dash_dir_x = 0.0f;
            float dash_dir_y = 0.0f;

            // Determine dash direction based on movement keys (arrows)
            bool moving_up = IsKeyDown(KEY_UP);
            bool moving_down = IsKeyDown(KEY_DOWN);
            bool moving_left = IsKeyDown(KEY_LEFT);
            bool moving_right = IsKeyDown(KEY_RIGHT);

            if (moving_up) dash_dir_y = -1.0f;
            if (moving_down) dash_dir_y = 1.0f;
            if (moving_left) dash_dir_x = -1.0f;
            if (moving_right) dash_dir_x = 1.0f;

            if (dash_dir_x == 0.0f && dash_dir_y == 0.0f) {
                // If not moving, use aiming direction (WASD)
                bool aiming_up = IsKeyDown(KEY_W);
                bool aiming_down = IsKeyDown(KEY_S);
                bool aiming_left = IsKeyDown(KEY_A);
                bool aiming_right = IsKeyDown(KEY_D);

                if (aiming_up) dash_dir_y = -1.0f;
                if (aiming_down) dash_dir_y = 1.0f;
                if (aiming_left) dash_dir_x = -1.0f;
                if (aiming_right) dash_dir_x = 1.0f;
            }

            // Normalize dash direction to ensure consistent speed for diagonals
            float dash_length = sqrtf(dash_dir_x * dash_dir_x + dash_dir_y * dash_dir_y);
            if (dash_length > 0) {
                dash_dir_x /= dash_length;
                dash_dir_y /= dash_length;
            } else {
                // If no direction input at all, default to dashing upwards
                dash_dir_y = -1.0f;
            }

            // Calculate dash velocity
            float dash_speed_per_frame = PLAYER_DASH_DISTANCE / PLAYER_DASH_DURATION;
            player_dash_velocity_x = dash_dir_x * dash_speed_per_frame;
            player_dash_velocity_y = dash_dir_y * dash_speed_per_frame;
        }

        // Update player position based on dash or normal movement
        if (player_is_dashing) {
            player_x += player_dash_velocity_x * deltaTime;
            player_y += player_dash_velocity_y * deltaTime;

            // Check if dash duration has ended
            if (GetTime() > player_dash_end_time) {
                player_is_dashing = false;
                player_dash_velocity_x = 0.0f; // Stop dash movement
                player_dash_velocity_y = 0.0f;
                TraceLog(LOG_INFO, "Player dash ended.");
            }
        } else {
            // Normal movement (only if not stunned)
            player_vx = 0.0f;
            player_vy = 0.0f;
            if (!player_is_stunned) {
                if (IsKeyDown(KEY_UP)) player_vy = -player_speed;
                if (IsKeyDown(KEY_DOWN)) player_vy = player_speed;
                if (IsKeyDown(KEY_LEFT)) player_vx = -player_speed;
                if (IsKeyDown(KEY_RIGHT)) player_vx = player_speed;

                player_x += player_vx;
                player_y += player_vy;

                // Update aiming angle based on movement or aiming keys
                bool aiming_up = IsKeyDown(KEY_W);
                bool aiming_down = IsKeyDown(KEY_S);
                bool aiming_left = IsKeyDown(KEY_A);
                bool aiming_right = IsKeyDown(KEY_D);

                if (aiming_up || aiming_down || aiming_left || aiming_right) {
                    if (aiming_up && !aiming_left && !aiming_right) player_aim_angle = -PI / 2.0f;
                    else if (aiming_down && !aiming_left && !aiming_right) player_aim_angle = PI / 2.0f;
                    else if (aiming_left && !aiming_up && !aiming_down) player_aim_angle = PI;
                    else if (aiming_right && !aiming_up && !aiming_down) player_aim_angle = 0.0f;
                    else if (aiming_up && aiming_left) player_aim_angle = -3.0f * PI / 4.0f;
                    else if (aiming_up && aiming_right) player_aim_angle = -PI / 4.0f;
                    else if (aiming_down && aiming_left) player_aim_angle = 3.0f * PI / 4.0f;
                    else if (aiming_down && aiming_right) player_aim_angle = PI / 4.0f;
                }
                else if (player_vx != 0 || player_vy != 0) {
                    player_aim_angle = atan2f(player_vy, player_vx);
                }
            } else {
                // If player is stunned, check for stun end time
                if (GetTime() > player_stun_end_time) {
                    player_is_stunned = false;
                    TraceLog(LOG_INFO, "Player stun ended.");
                }
            }
        }

        // Player wrap-around (applies to both normal movement and dash movement)
        if (player_x < -player_size) player_x = (float)SCREEN_WIDTH;
        else if (player_x > SCREEN_WIDTH) player_x = -player_size;
        
        if (player_y < -player_size) player_y = (float)SCREEN_HEIGHT;
        else if (player_y > SCREEN_HEIGHT) player_y = -player_size;

        // --- Portal Mode Logic (Teleportation and Timed Deactivation) ---
        if (is_portal_mode) {
            // Portal deactivation over time
            if (portal_1_active && GetTime() > portal_active_until_time_1) {
                portal_1_active = false;
                TraceLog(LOG_INFO, "Portal 1 deactivated due to time.");
            }
            if (portal_2_active && GetTime() > portal_active_until_time_2) {
                portal_2_active = false;
                TraceLog(LOG_INFO, "Portal 2 deactivated due to time.");
            }

            // Teleport logic for player
            if (portal_1_active && portal_2_active && GetTime() - last_teleport_time >= TELEPORT_COOLDOWN) {
                Vector2 player_center = {player_x + player_size / 2.0f, player_y + player_size / 2.0f};

                // Check collision with Portal 1
                if (CheckCollisionCircles(player_center, player_size / 2.0f, portal_1_pos, PORTAL_RADIUS)) {
                    player_x = portal_2_pos.x - player_size / 2.0f;
                    player_y = portal_2_pos.y - player_size / 2.0f;
                    last_teleport_time = GetTime();
                    TraceLog(LOG_INFO, "Teleported player from Portal 1 to Portal 2.");
                }
                // Check collision with Portal 2
                else if (CheckCollisionCircles(player_center, player_size / 2.0f, portal_2_pos, PORTAL_RADIUS)) {
                    player_x = portal_1_pos.x - player_size / 2.0f;
                    player_y = portal_1_pos.y - player_size / 2.0f;
                    last_teleport_time = GetTime();
                    TraceLog(LOG_INFO, "Teleported player from Portal 2 to Portal 1.");
                }
            }

            // Teleport logic for obstacle (separate cooldown not needed if last_teleport_time is for any entity)
            // If separate cooldowns are needed for player and obstacle, new variables would be required.
            // For now, using the same last_teleport_time for simplicity, meaning if player teleports, obstacle can't immediately.
            if (portal_1_active && portal_2_active && GetTime() - last_teleport_time >= TELEPORT_COOLDOWN) {
                Vector2 obstacle_center = {obstacle_x + obstacle_size / 2.0f, obstacle_y + obstacle_size / 2.0f};

                // Check collision with Portal 1
                if (CheckCollisionCircles(obstacle_center, obstacle_size / 2.0f, portal_1_pos, PORTAL_RADIUS)) {
                    obstacle_x = portal_2_pos.x - obstacle_size / 2.0f;
                    obstacle_y = portal_2_pos.y - obstacle_size / 2.0f;
                    last_teleport_time = GetTime();
                    TraceLog(LOG_INFO, "Teleported obstacle from Portal 1 to Portal 2.");
                }
                // Check collision with Portal 2
                else if (CheckCollisionCircles(obstacle_center, obstacle_size / 2.0f, portal_2_pos, PORTAL_RADIUS)) {
                    obstacle_x = portal_1_pos.x - obstacle_size / 2.0f;
                    obstacle_y = portal_1_pos.y - obstacle_size / 2.0f;
                    last_teleport_time = GetTime();
                    TraceLog(LOG_INFO, "Teleported obstacle from Portal 2 to Portal 1.");
                }
            }
        }


        if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) {
            has_shot_this_game = true; // Player has shot, "Bullet Ballet Master" is now impossible this game
            if (GetTime() - player_last_shot_time >= PLAYER_SHOOT_COOLDOWN) {
                Projectile newProjectile;
                newProjectile.rect = {player_x + player_size / 2.0f - PROJECTILE_SIZE / 2.0f,
                                      player_y + player_size / 2.0f - PROJECTILE_SIZE / 2.0f,
                                      PROJECTILE_SIZE, PROJECTILE_SIZE};
                newProjectile.speed = PROJECTILE_SPEED;
                newProjectile.velocity.x = cosf(player_aim_angle);
                newProjectile.velocity.y = sinf(player_aim_angle);
                newProjectile.active = true;
                newProjectile.is_player_shot = true;
                newProjectile.bounces_remaining = MAX_PROJECTILE_BOUNCES;
                projectiles.push_back(newProjectile);
                player_last_shot_time = GetTime();
            }
        }

        if (!obstacle_is_stunned && GetTime() - obstacle_last_shot_time >= OBSTACLE_SHOOT_COOLDOWN) {
            Projectile obstacleProjectile;
            float obstacle_center_x = obstacle_x + obstacle_size / 2.0f;
            float obstacle_center_y = obstacle_y + obstacle_size / 2.0f;

            obstacleProjectile.rect = {obstacle_center_x - PROJECTILE_SIZE / 2.0f,
                                       obstacle_center_y - PROJECTILE_SIZE / 2.2f, // Slightly adjusted Y to originate from center
                                       PROJECTILE_SIZE, PROJECTILE_SIZE};
            obstacleProjectile.speed = OBSTACLE_PROJECTILE_SPEED;

            float angle_to_player = atan2f(player_y - obstacle_y, player_x - obstacle_x);
            obstacleProjectile.velocity.x = cosf(angle_to_player);
            obstacleProjectile.velocity.y = sinf(angle_to_player);

            float nudge_distance = obstacle_size / 2.0f + PROJECTILE_SIZE / 2.0f + 5.0f;
            obstacleProjectile.rect.x += obstacleProjectile.velocity.x * nudge_distance;
            obstacleProjectile.rect.y += obstacleProjectile.velocity.y * nudge_distance;

            obstacleProjectile.active = true;
            obstacleProjectile.is_player_shot = false;
            obstacleProjectile.bounces_remaining = MAX_PROJECTILE_BOUNCES;
            projectiles.push_back(obstacleProjectile);
            obstacle_last_shot_time = GetTime();
        }

        for (auto& p : projectiles) {
            if (p.active) {
                p.rect.x += p.velocity.x * p.speed;
                p.rect.y += p.velocity.y * p.speed;

                // --- Portal Creation Logic (if in portal mode and player shot) ---
                if (is_portal_mode && p.is_player_shot) {
                    bool hit_for_portal = false;
                    Vector2 portal_spawn_pos = {0,0};

                    // Calculate projectile center for accurate hit point
                    Vector2 projectile_center = {p.rect.x + p.rect.width / 2.0f, p.rect.y + p.rect.height / 2.0f};

                    // Check collision with screen edges for portal
                    // Use a small buffer to ensure it's "on" the edge, not just past it
                    float edge_buffer = 1.0f; // 1 pixel buffer
                    if (p.rect.x <= edge_buffer || p.rect.x + p.rect.width >= SCREEN_WIDTH - edge_buffer ||
                        p.rect.y <= edge_buffer || p.rect.y + p.rect.height >= SCREEN_HEIGHT - edge_buffer) {
                        hit_for_portal = true;
                        // Determine exact impact point for portal placement
                        // Clamp to screen edges for precise placement
                        portal_spawn_pos.x = fmaxf(0.0f, fminf((float)SCREEN_WIDTH, projectile_center.x));
                        portal_spawn_pos.y = fmaxf(0.0f, fminf((float)SCREEN_HEIGHT, projectile_center.y));
                    }

                    // Check collision with obstacle for portal
                    if (!hit_for_portal && CheckCollisionRecs(p.rect, {obstacle_x, obstacle_y, obstacle_size, obstacle_size})) {
                        hit_for_portal = true;
                        portal_spawn_pos = {obstacle_x + obstacle_size / 2.0f, obstacle_y + obstacle_size / 2.0f};
                    }

                    if (hit_for_portal) {
                        p.active = false; // Deactivate projectile as it created a portal
                        
                        if (next_projectile_portal_is_1) {
                            portal_1_active = true;
                            portal_1_pos = portal_spawn_pos;
                            portal_active_until_time_1 = GetTime() + PORTAL_ACTIVE_DURATION;
                            next_projectile_portal_is_1 = false; // Next one will be portal 2
                            TraceLog(LOG_INFO, "Portal 1 created by projectile at (%.1f, %.1f)", portal_1_pos.x, portal_1_pos.y);
                        } else {
                            portal_2_active = true;
                            portal_2_pos = portal_spawn_pos;
                            portal_active_until_time_2 = GetTime() + PORTAL_ACTIVE_DURATION;
                            next_projectile_portal_is_1 = true; // Next one will be portal 1
                            TraceLog(LOG_INFO, "Portal 2 created by projectile at (%.1f, %.1f)", portal_2_pos.x, portal_2_pos.y);
                        }
                        continue; // Skip further processing for this projectile (no bouncing)
                    }
                }
                // --- End Portal Creation Logic ---

                // --- Projectile Boundary Handling (Bouncing then Wrapping) ---
                bool bounced_on_x = false;
                bool bounced_on_y = false;

                // Check X-axis for bouncing
                if (p.bounces_remaining > 0) {
                    if (p.rect.x < 0) { // Hit left edge
                        p.rect.x = 0; // Place exactly on edge
                        p.velocity.x *= -1; // Reverse velocity
                        p.bounces_remaining--;
                        bounced_on_x = true;
                    } else if (p.rect.x + p.rect.width > SCREEN_WIDTH) { // Hit right edge
                        p.rect.x = (float)SCREEN_WIDTH - p.rect.width; // Place exactly on edge
                        p.velocity.x *= -1; // Reverse velocity
                        p.bounces_remaining--;
                        bounced_on_x = true;
                    }
                }
                
                // Check Y-axis for bouncing
                if (p.bounces_remaining > 0) { // Re-check bounces_remaining in case X-bounce used one up
                    if (p.rect.y < 0) { // Hit top edge
                        p.rect.y = 0; // Place exactly on edge
                        p.velocity.y *= -1; // Reverse velocity
                        p.bounces_remaining--;
                        bounced_on_y = true;
                    } else if (p.rect.y + p.rect.height > SCREEN_HEIGHT) { // Hit bottom edge
                        p.rect.y = (float)SCREEN_HEIGHT - p.rect.height; // Place exactly on edge
                        p.velocity.y *= -1; // Reverse velocity
                        p.bounces_remaining--;
                        bounced_on_y = true;
                    }
                }

                // If bounces are exhausted (or were already 0), apply wrap-around
                // This check needs to be separate to ensure it only happens *after* potential bounces
                if (p.bounces_remaining <= 0 && !bounced_on_x && !bounced_on_y) {
                    // Apply wrap-around for X
                    if (p.rect.x < -p.rect.width) {
                        p.rect.x = (float)SCREEN_WIDTH;
                    } else if (p.rect.x > SCREEN_WIDTH) {
                        p.rect.x = -p.rect.width;
                    }
                    
                    // Apply wrap-around for Y
                    if (p.rect.y < -p.rect.height) {
                        p.rect.y = (float)SCREEN_HEIGHT;
                    } else if (p.rect.y > SCREEN_HEIGHT) {
                        p.rect.y = -p.rect.height;
                    }
                }
                // --- End Projectile Boundary Handling ---
                
                // --- Check for projectile collision with obstacle (Stun Mechanic) ---
                if (CheckCollisionRecs(p.rect, {obstacle_x, obstacle_y, obstacle_size, obstacle_size})) {
                    p.active = false;
                    dodge_streak_start_time = GetTime(); // Reset streak on hit
                    if (p.is_player_shot && GetTime() > player_last_stun_shot_time + PLAYER_STUN_SHOT_COOLDOWN) {
                        obstacle_is_stunned = true;
                        obstacle_stun_end_time = GetTime() + OBSTACLE_STUN_DURATION;
                        player_last_stun_shot_time = GetTime();
                        obstacle_stuns_this_game++; // Increment stun count for achievement
                        if (obstacle_stuns_this_game >= 3) {
                            unlockAchievement("stunned_silence", current_username);
                        }
                        TraceLog(LOG_INFO, "Obstacle stunned for %.1f seconds!", OBSTACLE_STUN_DURATION);
                    }
                }

                // --- Check for obstacle projectile collision with player (Stun Player) ---
                // Only stun player if not dashing
                if (!player_is_dashing && !p.is_player_shot && CheckCollisionRecs(p.rect, {player_x, player_y, player_size, player_size})) {
                    p.active = false;
                    dodge_streak_start_time = GetTime(); // Reset streak on hit
                    if (GetTime() > obstacle_last_stun_time + OBSTACLE_STUN_COOLDOWN) {
                        player_is_stunned = true;
                        player_stun_end_time = GetTime() + PLAYER_STUN_DURATION;
                        obstacle_last_stun_time = GetTime();
                        TraceLog(LOG_INFO, "Player stunned for %.1f seconds by obstacle projectile!", PLAYER_STUN_DURATION);
                    }
                }

                // --- "Dash of Genius" achievement check ---
                // If player is dashing and this is an obstacle projectile, check for collision
                if (player_is_dashing && !p.is_player_shot && CheckCollisionRecs(p.rect, {player_x, player_y, player_size, player_size})) {
                    p.active = false; // Projectile is "dodged" by dash
                    if (!dash_through_projectile_achievement_unlocked_this_game) {
                        unlockAchievement("dash_of_genius", current_username);
                        dash_through_projectile_achievement_unlocked_this_game = true; // Only unlock once per game
                    }
                }

                // --- "Near Miss" achievement check ---
                // Check if it's an obstacle projectile and not a direct hit, and player is not dashing
                if (!near_miss_achievement_unlocked_this_game && !p.is_player_shot && !player_is_dashing) {
                    float player_center_x = player_x + player_size / 2.0f;
                    float player_center_y = player_y + player_size / 2.0f;
                    float projectile_center_x = p.rect.x + p.rect.width / 2.0f;
                    float projectile_center_y = p.rect.y + p.rect.height / 2.0f;

                    float dx = player_center_x - projectile_center_x;
                    float dy = player_center_y - projectile_center_y;
                    float distance = sqrtf(dx*dx + dy*dy);

                    // Define a "near miss" threshold (e.g., within 1.5 times the combined radius, but not a direct hit)
                    float combined_radius = (player_size / 2.0f) + (PROJECTILE_SIZE / 2.0f);
                    float near_miss_threshold_distance = combined_radius + 15.0f; // 15 pixels beyond direct collision

                    // If it's very close but not colliding AND it's an obstacle projectile
                    if (distance > combined_radius && distance < near_miss_threshold_distance) {
                        unlockAchievement("near_miss", current_username);
                        near_miss_achievement_unlocked_this_game = true; // Unlock once per game
                    }
                }
            }
        }
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p){ return !p.active; }), projectiles.end());

        ai_reaction_timer++;
        if (ai_reaction_timer >= ai_reaction_delay) {
            float raw_predicted_player_x = player_x + (player_vx * prediction_frames);
            float raw_predicted_player_y = player_y + (player_vy * prediction_frames);

            float dx_direct = raw_predicted_player_x - obstacle_x;
            float dx_wrap_left = (raw_predicted_player_x + SCREEN_WIDTH) - obstacle_x;
            float dx_wrap_right = raw_predicted_player_x - (obstacle_x + SCREEN_WIDTH);

            float shortest_dx = dx_direct;
            if (std::abs(dx_wrap_left) < std::abs(shortest_dx)) {
                shortest_dx = dx_wrap_left;
            }
            if (std::abs(dx_wrap_right) < std::abs(shortest_dx)) {
                shortest_dx = dx_wrap_right;
            }
            ai_target_x = obstacle_x + shortest_dx;

            float dy_direct = raw_predicted_player_y - obstacle_y;
            float dy_wrap_up = (raw_predicted_player_y + SCREEN_HEIGHT) - obstacle_y;
            float dy_wrap_down = raw_predicted_player_y - (obstacle_y + SCREEN_HEIGHT);

            float shortest_dy = dy_direct;
            if (std::abs(dy_wrap_up) < std::abs(shortest_dy)) {
                shortest_dy = dy_wrap_up;
            }
            if (std::abs(dy_wrap_down) < std::abs(shortest_dy)) {
                shortest_dy = dy_wrap_down;
            }
            ai_target_y = obstacle_y + shortest_dy;

            ai_reaction_timer = 0;
        }

        if (!obstacle_is_stunned) {
            if (obstacle_x < ai_target_x) {
                obstacle_x += obstacle_speed;
            } else if (obstacle_x > ai_target_x) {
                obstacle_x -= obstacle_speed;
            }
            if (obstacle_y < ai_target_y) {
                obstacle_y += obstacle_speed;
            } else if (obstacle_y > ai_target_y) {
                obstacle_y -= obstacle_speed;
            }
        } else {
            if (GetTime() > obstacle_stun_end_time) {
                obstacle_is_stunned = false;
                TraceLog(LOG_INFO, "Obstacle stun ended.");
            }
        }

        if (obstacle_x < -obstacle_size) obstacle_x = (float)SCREEN_WIDTH;
        else if (obstacle_x > SCREEN_WIDTH) obstacle_x = -obstacle_size;
        
        if (obstacle_y < -obstacle_size) obstacle_y = (float)SCREEN_HEIGHT;
        else if (obstacle_y > SCREEN_HEIGHT) obstacle_y = -obstacle_size;

        // --- Dodge Streak Time Bonus Logic ---
        // Only award bonus if player is not stunned and hasn't just been hit
        if (!player_is_stunned) {
            if (GetTime() - dodge_streak_start_time >= DODGE_BONUS_INTERVAL) {
                elapsed_time_s += DODGE_BONUS_AMOUNT;
                dodge_streak_start_time = GetTime(); // Reset streak timer for next bonus
                showing_time_bonus_message = true;
                time_bonus_message_end_time = GetTime() + TIME_BONUS_MESSAGE_DURATION;
                TraceLog(LOG_INFO, "Time Bonus! +%.1f seconds. New elapsed time: %.2f", DODGE_BONUS_AMOUNT, elapsed_time_s);
            }
        }

        // Hide time bonus message after its duration
        if (showing_time_bonus_message && GetTime() > time_bonus_message_end_time) {
            showing_time_bonus_message = false;
        }

        // Handle achievement popup visibility
        if (!current_achievement_popup_id.empty() && GetTime() > achievement_popup_display_end_time) {
            current_achievement_popup_id = ""; // Clear the popup
        }

        // Player vs Obstacle Collision (only if player is NOT dashing)
        if (!player_is_dashing && player_x < obstacle_x + obstacle_size &&
            player_x + player_size > obstacle_x &&
            player_y < obstacle_y + obstacle_size &&
            player_y + player_size > obstacle_y) {
            
            current_game_state = GAME_STATE_GAME_OVER;
            final_survival_time_s = elapsed_time_s;

            // --- CRITICAL FIX: Clear projectiles and reset dash state immediately on game over ---
            projectiles.clear();
            player_is_dashing = false;
            player_dash_velocity_x = 0.0f;
            player_dash_velocity_y = 0.0f;
            // --- END CRITICAL FIX ---

            const double currentWinThreshold = WIN_THRESHOLD_TIMES[current_difficulty_mode];
            bool didWin = final_survival_time_s >= currentWinThreshold;

            if (final_survival_time_s > high_scores[current_difficulty_mode]) {
                high_scores[current_difficulty_mode] = final_survival_time_s;
                is_new_high_score = true;
            } else {
                is_new_high_score = false;
            }
            TraceLog(LOG_INFO, "Game Over! Final Time: %.2f s, New High Score: %s", final_survival_time_s, is_new_high_score ? "YES" : "NO");
            TraceLog(LOG_INFO, "Current Difficulty High Score: %.2f s", high_scores[current_difficulty_mode]);

            if (didWin && is_new_high_score) {
                if (normal_music.frameCount > 0 && IsMusicStreamPlaying(normal_music)) {
                    StopMusicStream(normal_music);
                }
                if (win_music.frameCount > 0 && !IsMusicStreamPlaying(win_music)) {
                    PlayMusicStream(win_music);
                }
            } else {
                if (win_music.frameCount > 0 && IsMusicStreamPlaying(win_music)) {
                    StopMusicStream(win_music);
                }
            }
        }

    } else if (current_game_state == GAME_STATE_GAME_OVER) {
        if (IsKeyPressed(KEY_R)) {
            TraceLog(LOG_INFO, "R key pressed. Attempting to restart/advance difficulty.");
            const double currentWinThreshold = WIN_THRESHOLD_TIMES[current_difficulty_mode];
            const bool didWin = final_survival_time_s >= currentWinThreshold;

            if (didWin) {
                nextGameMessage = "YOU HAVE BEATEN THE GAME ON NORMAL MODE!";
            } else {
                nextGameMessage = "TRY AGAIN!";
            }
            applyDifficulty("normal");
            resetGame();
            current_game_state = GAME_STATE_COUNTDOWN; // Go straight to countdown after game over restart
        }
        if (IsKeyPressed(KEY_P)) { // P to change profile from Game Over screen
            current_game_state = GAME_STATE_USERNAME_INPUT;
            username_input_buffer = current_username;
            if (normal_music.frameCount > 0 && IsMusicStreamPlaying(normal_music)) {
                StopMusicStream(normal_music);
            }
            if (win_music.frameCount > 0 && IsMusicStreamPlaying(win_music)) {
                StopMusicStream(win_music);
            }
        }

        if (current_difficulty_mode == "babymode" && final_survival_time_s < 10.0 && high_scores["babymode"] > 20.0) {
            // Rickroll placeholder
        }
    } else if (current_game_state == GAME_STATE_ACHIEVEMENTS) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            current_game_state = GAME_STATE_MAIN_MENU; // Go back to main menu
        }
    } else if (current_game_state == GAME_STATE_SELECT_ACHIEVEMENT_PROFILE) {
        drawSelectAchievementProfileScreen();
    }
}

void drawCenteredText(const std::string& text, int fontSize, Color color, int yOffset) {
    int textWidth = MeasureText(text.c_str(), fontSize);
    DrawText(text.c_str(), (SCREEN_WIDTH - textWidth) / 2, (SCREEN_HEIGHT - fontSize) / 2 + yOffset, fontSize, color);
}

void drawInfoText(const std::string& text, int fontSize, Color color, int x, int y, TextAlignment align) {
    int textWidth = MeasureText(text.c_str(), fontSize);
    int drawX = x;
    if (align == ALIGN_CENTER) {
        drawX = x - textWidth / 2;
    } else if (align == ALIGN_RIGHT) {
        drawX = x - textWidth;
    }
    DrawText(text.c_str(), drawX, y, fontSize, color);
}

void drawAchievementsScreen() {
    ClearBackground(BLACK); // Clear the background for the new screen
    int current_y_offset = 50;

    drawCenteredText("ACHIEVEMENTS", 60, GOLD, current_y_offset - (SCREEN_HEIGHT/2 - 60/2));
    current_y_offset += 100;

    // Check if the current user has any unlocked achievements
    const auto& user_unlocked_achievements = UNLOCKED_ACHIEVEMENTS_BY_USER[current_username];

    for (const auto& pair : ALL_ACHIEVEMENTS) {
        const Achievement& achievement = pair.second;
        bool is_unlocked = false;
        // Check if this specific achievement ID is in the current user's unlocked list
        for (const auto& unlocked_id : user_unlocked_achievements) {
            if (unlocked_id == achievement.id) {
                is_unlocked = true;
                break;
            }
        }

        Color display_color = is_unlocked ? UNLOCKED_ACHIEVEMENT_COLOR : LOCKED_ACHIEVEMENT_COLOR;
        std::string display_name;
        std::string display_description;

        if (achievement.is_secret && !is_unlocked) {
            display_name = "??? Secret Achievement ???";
            display_description = "Unlock this to reveal its purpose!";
        } else {
            display_name = achievement.name;
            display_description = achievement.description;
        }
        
        std::string status_prefix = is_unlocked ? "[UNLOCKED] " : "[LOCKED]   ";

        DrawText((status_prefix + display_name).c_str(), 50, current_y_offset, 30, display_color);
        current_y_offset += 35;
        DrawText(("  - " + display_description).c_str(), 70, current_y_offset, 20, display_color);
        current_y_offset += 50; // More space between achievements
    }

    drawCenteredText("Press ESC to go back", 25, WHITE, SCREEN_HEIGHT - 50 - (SCREEN_HEIGHT/2 - 25/2));
}

void drawSelectAchievementProfileScreen() {
    ClearBackground(BLACK);
    int current_y_offset = 50;

    drawCenteredText("SELECT PROFILE FOR 'PORTAL' ACHIEVEMENT", 40, GOLD, current_y_offset - (SCREEN_HEIGHT/2 - 40/2));
    current_y_offset += 80;

    if (available_profile_names.empty()) {
        drawCenteredText("No profiles found. Create one first!", 25, RED, current_y_offset - (SCREEN_HEIGHT/2 - 25/2));
    } else {
        for (size_t i = 0; i < available_profile_names.size(); ++i) {
            Color color = (i == selected_profile_index) ? SELECTED_ITEM_COLOR : LIGHTGRAY_CUSTOM;
            int fontSize = (i == selected_profile_index) ? 35 : 30;
            drawCenteredText(available_profile_names[i], fontSize, color, current_y_offset - (SCREEN_HEIGHT/2 - fontSize/2));
            current_y_offset += 45;
        }
    }

    drawCenteredText("Use UP/DOWN arrows to select, ENTER to confirm.", 25, WHITE, SCREEN_HEIGHT - 80 - (SCREEN_HEIGHT/2 - 25/2));
    drawCenteredText("Press ESC to go back to username input.", 20, WHITE, SCREEN_HEIGHT - 50 - (SCREEN_HEIGHT/2 - 20/2));
}


void drawGame() {
    if (current_game_state == GAME_STATE_TAMPERED) {
        drawCenteredText("ARE YOU HAPPY THAT YOU'RE A CHEATER?", 40, RED, -50);
        drawCenteredText("Game will close shortly.", 20, WHITE, 20);
    } else if (current_game_state == GAME_STATE_USERNAME_INPUT) {
        // Define font sizes and padding for clarity
        int title_font_size = 60;
        int input_text_font_size = 28;
        int confirm_font_size = 30;
        int disclaimer_font_size = 22;
        // Removed instructions_font_size as "Press A" is no longer here

        int input_box_width = 500;
        int input_box_height = 60;
        int vertical_padding = 40; // Padding between main elements

        // Calculate initial Y for the title, aiming to center the whole block
        // Adjusted total_height_of_main_elements as "Press A" instruction is removed
        int total_height_of_main_elements = title_font_size + vertical_padding + input_box_height + vertical_padding + confirm_font_size;
        int current_y = (SCREEN_HEIGHT / 2) - (total_height_of_main_elements / 2);

        // "ENTER USERNAME" title
        DrawText("ENTER USERNAME", (int)(SCREEN_WIDTH / 2 - MeasureText("ENTER USERNAME", title_font_size) / 2), current_y, title_font_size, GOLD);
        current_y += title_font_size + vertical_padding; // Move Y down for next element

        // Input box
        int input_box_x = (SCREEN_WIDTH - input_box_width) / 2;
        DrawRectangle(input_box_x, current_y, input_box_width, input_box_height, LIGHTGRAY);
        DrawRectangleLines(input_box_x, current_y, input_box_width, input_box_height, WHITE);
        // Center text vertically within the input box
        DrawText(username_input_buffer.c_str(), input_box_x + 15, current_y + (input_box_height - input_text_font_size) / 2, input_text_font_size, BLACK);
        current_y += input_box_height + vertical_padding; // Move Y down for next element

        // "Press ENTER to confirm" message
        DrawText("Press ENTER to confirm", (int)(SCREEN_WIDTH / 2 - MeasureText("Press ENTER to confirm", confirm_font_size) / 2), current_y, confirm_font_size, WHITE);
        // current_y += confirm_font_size + vertical_padding; // No need to move Y down for next element here

        // Disclaimer text (fixed at bottom, with some padding from the bottom edge)
        const Color disclaimer_color = YELLOW; // Redeclared here
        const std::string disclaimer_text_line1 = "WARNING: This game may contain rapidly flashing elements."; // Redeclared here
        const std::string disclaimer_text_line2 = "Players with photosensitive epilepsy should exercise caution."; // Redeclared here

        int disclaimer_bottom_margin = 50;
        int disclaimer_line_spacing = 30;
        int disclaimer_y2 = SCREEN_HEIGHT - disclaimer_bottom_margin;
        int disclaimer_y1 = disclaimer_y2 - disclaimer_line_spacing;

        drawCenteredText(disclaimer_text_line1, disclaimer_font_size, disclaimer_color, disclaimer_y1 - (SCREEN_HEIGHT/2 - disclaimer_font_size/2));
        drawCenteredText(disclaimer_text_line2, disclaimer_font_size, disclaimer_color, disclaimer_y2 - (SCREEN_HEIGHT/2 - disclaimer_font_size/2));


    } else if (current_game_state == GAME_STATE_MAIN_MENU) {
        int current_y = (SCREEN_HEIGHT / 2) - 100; // Adjusted start Y

        drawCenteredText("WELCOME, " + current_username + "!", 50, GOLD, current_y - (SCREEN_HEIGHT/2 - 50/2));
        current_y += 100;

        Color play_color = WHITE;
        Color achievements_color = LIGHTGRAY_CUSTOM;
        // Removed profile_color as the text is no longer drawn here

        // No visual feedback for key press, just the text itself
        drawCenteredText("PLAY GAME (Press ENTER)", 40, play_color, current_y - (SCREEN_HEIGHT/2 - 40/2));
        current_y += 60;
        drawCenteredText("VIEW ACHIEVEMENTS (Press A)", 30, achievements_color, current_y - (SCREEN_HEIGHT/2 - 30/2));
        // Removed the "Press P to Change Profile" text line from here
        current_y += 80; // Maintain spacing for consistency

        drawCenteredText("Press ESC to go back to username input", 20, WHITE, SCREEN_HEIGHT - 50 - (SCREEN_HEIGHT/2 - 20/2));


    } else if (current_game_state == GAME_STATE_COUNTDOWN) {
        int display_countdown = (current_countdown_frame / FPS) + 1;
        if (display_countdown > 0) {
            drawCenteredText(std::to_string(display_countdown), 100, WHITE);
        } else {
            drawCenteredText("GO!", 100, GREEN);
        }
    } else if (current_game_state == GAME_STATE_PLAYING) {
        // Top-Left: Profile Info
        drawInfoText("Profile: " + current_username + " (" + current_difficulty_mode + ")", 24, WHITE, 20, 20, ALIGN_LEFT);

        // Top-Center: Current Time
        std::string time_display_str = "Time: " + std::to_string(elapsed_time_s).substr(0, std::to_string(elapsed_time_s).find('.') + 2) + "s";
        drawInfoText(time_display_str, 36, WHITE, SCREEN_WIDTH / 2, 20, ALIGN_CENTER);

        // Top-Right: High Score or Time Left to Win
        if (high_scores["normal"] >= WIN_THRESHOLD_TIMES["normal"]) {
            std::string highScoreDisplay = "High Score: " + std::to_string(high_scores["normal"]).substr(0, std::to_string(high_scores["normal"]).find('.') + 3) + "s";
            drawInfoText(highScoreDisplay, 24, GOLD, SCREEN_WIDTH - 20, 20, ALIGN_RIGHT);
        } else {
            const double timeLeft = std::max(0.0, WIN_THRESHOLD_TIMES["normal"] - elapsed_time_s);
            std::string timeLeftDisplay_str = "Time to Win: " + std::to_string(timeLeft).substr(0, std::to_string(timeLeft).find('.') + 2) + "s";
            drawInfoText(timeLeftDisplay_str, 24, GOLD, SCREEN_WIDTH - 20, 20, ALIGN_RIGHT);
        }
        
        // Draw player and obstacle
        DrawRectangle(static_cast<int>(player_x), static_cast<int>(player_y),
                      static_cast<int>(player_size), static_cast<int>(player_size),
                      player_is_stunned ? LIGHTGRAY_CUSTOM : player_color);

        Vector2 playerCenter = {player_x + player_size / 2.0f, player_y + player_size / 2.0f};
        float aimLineLength = player_size * 1.5f;
        Vector2 aimLineEnd = {
            playerCenter.x + cosf(player_aim_angle) * aimLineLength,
            playerCenter.y + sinf(player_aim_angle) * aimLineLength
        };
        DrawLineV(playerCenter, aimLineEnd, WHITE);

        DrawRectangle(static_cast<int>(obstacle_x), static_cast<int>(obstacle_y),
                      static_cast<int>(obstacle_size), static_cast<int>(obstacle_size), 
                      obstacle_is_stunned ? OBSTACLE_STUNNED_COLOR : obstacle_color);

        // Draw projectiles
        for (const auto& p : projectiles) {
            if (p.active) {
                DrawRectangleRec(p.rect, p.is_player_shot ? PROJECTILE_COLOR : OBSTACLE_PROJECTILE_COLOR);
            }
        }

        // --- Draw Portals if active ---
        if (is_portal_mode) {
            if (portal_1_active) {
                DrawCircleV(portal_1_pos, PORTAL_RADIUS, PORTAL_COLOR_1);
            }
            if (portal_2_active) {
                DrawCircleV(portal_2_pos, PORTAL_RADIUS, PORTAL_COLOR_2);
            }
        }

        // --- Draw Time Bonus Message ---
        if (showing_time_bonus_message) {
            drawCenteredText("TIME BONUS +1s!", 50, GREEN, 0); // Centered, large green text
        }

        // --- Draw Achievement Popup ---
        if (!current_achievement_popup_id.empty()) {
            const Achievement& popup_achievement = ALL_ACHIEVEMENTS.at(current_achievement_popup_id);
            std::string popup_name = popup_achievement.name;
            std::string popup_desc = popup_achievement.description;

            int popup_width = 600;
            int popup_height = 150;
            int popup_x = (SCREEN_WIDTH - popup_width) / 2;
            int popup_y = (SCREEN_HEIGHT - popup_height) / 2; // Centered vertically

            DrawRectangle(popup_x, popup_y, popup_width, popup_height, Fade(BLACK, 0.8f));
            DrawRectangleLines(popup_x, popup_y, popup_width, popup_height, UNLOCKED_ACHIEVEMENT_COLOR);

            int text_start_y = popup_y + 20; // Padding from top of popup

            int title_font_size = 30;
            int name_font_size = 25;
            int desc_font_size = 20;

            // "ACHIEVEMENT UNLOCKED!"
            int text_width_title = MeasureText("ACHIEVEMENT UNLOCKED!", title_font_size);
            DrawText("ACHIEVEMENT UNLOCKED!", popup_x + (popup_width - text_width_title) / 2, text_start_y, title_font_size, UNLOCKED_ACHIEVEMENT_COLOR);
            text_start_y += title_font_size + 10;

            // Achievement Name
            int text_width_name = MeasureText(popup_name.c_str(), name_font_size);
            DrawText(popup_name.c_str(), popup_x + (popup_width - text_width_name) / 2, text_start_y, name_font_size, WHITE);
            text_start_y += name_font_size + 10;

            // Achievement Description
            int text_width_desc = MeasureText(popup_desc.c_str(), desc_font_size);
            DrawText(popup_desc.c_str(), popup_x + (popup_width - text_width_desc) / 2, text_start_y, desc_font_size, LIGHTGRAY);
        }

        // Bottom-Center: Cooldowns
        int cooldown_y_offset = SCREEN_HEIGHT - 30; // Starting Y for cooldowns, from bottom
        const int cooldown_line_height = 25; // Spacing between cooldown lines

        if (GetTime() - player_last_dash_time < PLAYER_DASH_COOLDOWN) {
            double cooldown_remaining = PLAYER_DASH_COOLDOWN - (GetTime() - player_last_dash_time);
            std::string cooldown_str = "Dash CD: " + std::to_string(cooldown_remaining).substr(0, std::to_string(cooldown_remaining).find('.') + 2) + "s";
            drawInfoText(cooldown_str, 20, BLUE, SCREEN_WIDTH / 2, cooldown_y_offset, ALIGN_CENTER);
            cooldown_y_offset -= cooldown_line_height;
        }

        if (GetTime() - player_last_stun_shot_time < PLAYER_STUN_SHOT_COOLDOWN) {
            double cooldown_remaining = PLAYER_STUN_SHOT_COOLDOWN - (GetTime() - player_last_stun_shot_time);
            std::string cooldown_str = "Player Stun Shot CD: " + std::to_string(cooldown_remaining).substr(0, std::to_string(cooldown_remaining).find('.') + 2) + "s";
            drawInfoText(cooldown_str, 20, ORANGE, SCREEN_WIDTH / 2, cooldown_y_offset, ALIGN_CENTER);
            cooldown_y_offset -= cooldown_line_height;
        }

        if (GetTime() - obstacle_last_stun_time < OBSTACLE_STUN_COOLDOWN) {
            double cooldown_remaining = OBSTACLE_STUN_COOLDOWN - (GetTime() - obstacle_last_stun_time);
            std::string cooldown_str = "Obstacle Stun CD: " + std::to_string(cooldown_remaining).substr(0, std::to_string(cooldown_remaining).find('.') + 2) + "s";
            drawInfoText(cooldown_str, 20, RED, SCREEN_WIDTH / 2, cooldown_y_offset, ALIGN_CENTER);
            cooldown_y_offset -= cooldown_line_height;
        }

        if (GetTime() - obstacle_last_shot_time < OBSTACLE_SHOOT_COOLDOWN) {
            double cooldown_remaining = OBSTACLE_SHOOT_COOLDOWN - (GetTime() - obstacle_last_shot_time);
            std::string cooldown_str = "Obstacle Shoot CD: " + std::to_string(cooldown_remaining).substr(0, std::to_string(cooldown_remaining).find('.') + 2) + "s";
            drawInfoText(cooldown_str, 20, OBSTACLE_PROJECTILE_COLOR, SCREEN_WIDTH / 2, cooldown_y_offset, ALIGN_CENTER);
            cooldown_y_offset -= cooldown_line_height;
        }


    } else if (current_game_state == GAME_STATE_GAME_OVER) {
        const float center_x = (float)SCREEN_WIDTH / 2.0f;
        const float center_y = (float)SCREEN_HEIGHT / 2.0f;

        // Redeclare variables for this scope
        double currentProfileHighScore = high_scores[current_difficulty_mode];
        const double currentWinThreshold = WIN_THRESHOLD_TIMES[current_difficulty_mode];
        const bool didWin = final_survival_time_s >= currentWinThreshold;

        // Start Y for the first element, adjusted to center the entire block of text
        int current_y_pos = (SCREEN_HEIGHT / 2) - 250; // Adjusted starting Y to move content up

        // "GAME OVER!" title
        int game_over_font_size = 130;
        DrawText("GAME OVER!", (int)(center_x - MeasureText("GAME OVER!", game_over_font_size) / 2), current_y_pos, game_over_font_size, RED);
        current_y_pos += game_over_font_size + 30; // Move Y down, add padding

        // Profile Info
        int profile_font_size = 40;
        std::string profile_text = "Profile: " + current_username + " (" + current_difficulty_mode + ")";
        DrawText(profile_text.c_str(), (int)(center_x - MeasureText(profile_text.c_str(), profile_font_size) / 2), current_y_pos, profile_font_size, WHITE);
        current_y_pos += profile_font_size + 40; // Move Y down, add more padding

        // Score/Time Display
        if (is_new_high_score) {
            int new_best_font_size = 80;
            std::string new_best_text = "NEW BEST: " + std::to_string(final_survival_time_s).substr(0, std::to_string(final_survival_time_s).find('.') + 3) + " seconds!";
            DrawText(new_best_text.c_str(), (int)(center_x - MeasureText(new_best_text.c_str(), new_best_font_size) / 2), current_y_pos, new_best_font_size, GOLD);
            current_y_pos += new_best_font_size + 30;
        } else {
            int final_time_font_size = 60;
            std::string final_time_text = "Your Time: " + std::to_string(final_survival_time_s).substr(0, std::to_string(final_survival_time_s).find('.') + 3) + " seconds!";
            DrawText(final_time_text.c_str(), (int)(center_x - MeasureText(final_time_text.c_str(), final_time_font_size) / 2), current_y_pos, final_time_font_size, WHITE);
            current_y_pos += final_time_font_size + 30;
        }

        // High Score / Time Needed to Win
        int score_info_font_size = 48;
        if (currentProfileHighScore >= currentWinThreshold) {
            std::string highScoreDisplay = "Your High Score: " + std::to_string(currentProfileHighScore).substr(0, std::to_string(currentProfileHighScore).find('.') + 3) + "s";
            DrawText(highScoreDisplay.c_str(), (int)(center_x - MeasureText(highScoreDisplay.c_str(), score_info_font_size) / 2), current_y_pos, score_info_font_size, GOLD);
            current_y_pos += score_info_font_size + 30;
        } else {
            if (!didWin) {
                std::string time_needed_text = "You needed " + std::to_string(currentWinThreshold - final_survival_time_s).substr(0, std::to_string(currentWinThreshold - final_survival_time_s).find('.') + 3) + " more seconds to win!";
                DrawText(time_needed_text.c_str(), (int)(center_x - MeasureText(time_needed_text.c_str(), score_info_font_size) / 2), current_y_pos, score_info_font_size, GOLD);
                current_y_pos += score_info_font_size + 30;
            }
        }

        // Win/Try Again message - MODIFIED LOGIC HERE
        int game_message_font_size = 52;
        if (didWin && is_new_high_score) { // Only show "BEATEN THE GAME" if it's a new high score AND a win
            nextGameMessage = "YOU HAVE BEATEN THE GAME ON NORMAL MODE!";
        } else {
            nextGameMessage = "TRY AGAIN!"; // Otherwise, default to "TRY AGAIN!"
        }
        DrawText(nextGameMessage.c_str(), (int)(center_x - MeasureText(nextGameMessage.c_str(), game_message_font_size) / 2), current_y_pos + 20, game_message_font_size, WHITE); // Add extra padding before this message
        current_y_pos += game_message_font_size + 50; // Move Y down

        // Instructions
        int instruction_font_size = 45;
        DrawText("Press R to Continue", (int)(center_x - MeasureText("Press R to Continue", instruction_font_size) / 2), current_y_pos + 20, instruction_font_size, WHITE);
        DrawText("Press P to Change Profile", (int)(center_x - MeasureText("Press P to Change Profile", instruction_font_size) / 2), current_y_pos + 80, instruction_font_size, WHITE);

        if (current_difficulty_mode == "babymode" && final_survival_time_s < 10.0 && high_scores["babymode"] > 20.0) {
            // Rickroll placeholder
        }
    } else if (current_game_state == GAME_STATE_ACHIEVEMENTS) {
        drawAchievementsScreen();
    } else if (current_game_state == GAME_STATE_SELECT_ACHIEVEMENT_PROFILE) {
        drawSelectAchievementProfileScreen();
    }
}
