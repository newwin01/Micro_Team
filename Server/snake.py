import pygame
import random
import sys
import threading
import select

# Initialize Pygame
pygame.init()

# Screen dimensions
screen_width = 800
screen_height = 600

# Colors
white = (255, 255, 255)
black = (0, 0, 0)
green = (0, 255, 0)
red = (255, 0, 0)

# Snake and food dimensions
snake_size = 20
food_size = 20

# Speeds
snake_speed = 2

# Create the screen
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Snake")

# Clock for controlling the frame rate
clock = pygame.time.Clock()

def draw_snake(snake_body):
    for segment in snake_body:
        pygame.draw.rect(screen, green, pygame.Rect(segment[0], segment[1], snake_size, snake_size))

def draw_food(food):
    pygame.draw.rect(screen, red, pygame.Rect(food[0], food[1], food_size, food_size))

def calculate_overlap_area(rect1, rect2):
    left = max(rect1[0], rect2[0])
    right = min(rect1[0] + rect1[2], rect2[0] + rect2[2])
    top = max(rect1[1], rect2[1])
    bottom = min(rect1[1] + rect1[3], rect2[1] + rect2[3])

    if left < right and top < bottom:
        return (right - left) * (bottom - top)
    else:
        return 0

# Global variable to track if the game should end
terminate_game = False

def read_messages():
    global terminate_game, direction
    while True:
        message = sys.stdin.buffer.readline().rstrip()  # Read the message as bytes and remove trailing newline

        if "END" in message.decode('utf-8'):
            print("Received termination signal. Exiting...")
            terminate_game = True
            break
        elif "UP" in message.decode('utf-8') and direction != 'DOWN':
            direction = 'UP'
        elif "DOWN" in message.decode('utf-8') and direction != 'UP':
            direction = 'DOWN'
        elif "LEFT" in message.decode('utf-8') and direction != 'RIGHT':
            direction = 'LEFT'
        elif "RIGHT" in message.decode('utf-8') and direction != 'LEFT':
            direction = 'RIGHT'

def game_loop():
    global direction

    # Initialize snake and food
    snake_body = [[100, 50], [90, 50], [80, 50]]
    direction = 'RIGHT'
    food = [random.randrange(1, screen_width // snake_size) * snake_size,
            random.randrange(1, screen_height // snake_size) * snake_size]

    player_score = 0
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False, player_score

        if terminate_game:
            return False, player_score

        # Move the snake
        if direction == 'UP':
            new_head = [snake_body[0][0], snake_body[0][1] - snake_speed]
        elif direction == 'DOWN':
            new_head = [snake_body[0][0], snake_body[0][1] + snake_speed]
        elif direction == 'LEFT':
            new_head = [snake_body[0][0] - snake_speed, snake_body[0][1]]
        elif direction == 'RIGHT':
            new_head = [snake_body[0][0] + snake_speed, snake_body[0][1]]

        # Insert new head
        snake_body.insert(0, new_head)

        # Check if snake eats food
        snake_head_rect = pygame.Rect(snake_body[0][0], snake_body[0][1], snake_size, snake_size)
        food_rect = pygame.Rect(food[0], food[1], food_size, food_size)

        overlap_area = calculate_overlap_area(snake_head_rect, food_rect)
        food_area = food_size * food_size

        if overlap_area >= 0.75 * food_area:
            player_score += 1
            food = [random.randrange(1, screen_width // snake_size) * snake_size,
                    random.randrange(1, screen_height // snake_size) * snake_size]
        else:
            snake_body.pop()

        # Check for collisions
        if (snake_body[0][0] not in range(screen_width) or
            snake_body[0][1] not in range(screen_height) or
            snake_body[0] in snake_body[1:]):
            return True, player_score

        screen.fill(black)
        draw_snake(snake_body)
        draw_food(food)

        pygame.display.flip()
        clock.tick(15)

def main():
    global terminate_game

    # Start a separate thread to read messages from stdin
    message_thread = threading.Thread(target=read_messages)
    message_thread.start()

    while not terminate_game:
        game_over, player_score = game_loop()
        if not game_over:
            break
        else:
            print(f"Game over! Your score was: {player_score}")
            pygame.time.wait(2000)  # Wait for 2 seconds before restarting

    pygame.quit()
    print(f"Final player score: {player_score}")
    sys.exit(player_score)

if __name__ == "__main__":
    main()
