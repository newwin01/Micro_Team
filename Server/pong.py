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

# Paddle dimensions
paddle_width = 10
paddle_height = 100

# Ball dimensions
ball_size = 10

# Speeds
ball_speed_x = 2
ball_speed_y = 2
paddle_speed = 10

# Create the screen
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Pong")

# Clock for controlling the frame rate
clock = pygame.time.Clock()

def draw_paddle(paddle):
    pygame.draw.rect(screen, white, paddle)

def draw_ball(ball):
    pygame.draw.ellipse(screen, white, ball)

# Global variable to track if the game should end
terminate_game = False

move_up = False
move_down = False

def read_messages():
    global terminate_game, move_up, move_down
    while True:
        message = sys.stdin.buffer.readline().rstrip()  # Read the message as bytes and remove trailing newline

        if message.decode('utf-8') == "END": 
            print("Received termination signal. Exiting...")
            terminate_game = True
            break
        elif message.decode('utf-8') == "UP":
            move_down = False
            move_up = True   # Set flag for moving paddle up continuously
        elif message.decode('utf-8') == "DOWN":
            move_up = False
            move_down = True  # Set flag for moving paddle down continuously

def move_left_paddle():
    global left_paddle, move_up, move_down
    if move_up and left_paddle.top > 0:
        left_paddle.y -= paddle_speed
    elif move_down and left_paddle.bottom < screen_height:
        left_paddle.y += paddle_speed

def main():
    global left_paddle, move_up, move_down

    # Start a separate thread to read messages from stdin
    message_thread = threading.Thread(target=read_messages)
    message_thread.start()

    # Initialize paddles and ball
    left_paddle = pygame.Rect(50, screen_height // 2 - paddle_height // 2, paddle_width, paddle_height)
    right_paddle = pygame.Rect(screen_width - 50 - paddle_width, screen_height // 2 - paddle_height // 2, paddle_width, paddle_height)
    ball = pygame.Rect(screen_width // 2 - ball_size // 2, screen_height // 2 - ball_size // 2, ball_size, ball_size)

    ball_dx = ball_speed_x * random.choice((-1, 1))
    ball_dy = ball_speed_y * random.choice((-1, 1))

    player_score = 0

    running = True
    terminate_game = False
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        if terminate_game:
            running = False
            continue

        # Check for input from stdin (if any)
        if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
            message = sys.stdin.buffer.readline().rstrip()
            if message:
                decoded_message = message.decode('utf-8')
                if decoded_message == "END":
                    print("Received termination signal. Exiting...")
                    terminate_game = True
                elif decoded_message == "UP":
                    move_up = True
                    move_down = False
                elif decoded_message == "DOWN":
                    move_down = True
                    move_up = False

        # Move the left paddle based on continuous movement flags
        move_left_paddle()

        # AI for the right paddle
        if right_paddle.centery < ball.centery and right_paddle.bottom < screen_height:
            right_paddle.y += paddle_speed
        if right_paddle.centery > ball.centery and right_paddle.top > 0:
            right_paddle.y -= paddle_speed

        ball.x += ball_dx
        ball.y += ball_dy

        if ball.top <= 0 or ball.bottom >= screen_height:
            ball_dy *= -1

        if ball.colliderect(left_paddle) or ball.colliderect(right_paddle):
            ball_dx *= -1
            player_score += 1

        if ball.left <= 0 or ball.right >= screen_width:
            ball.x = screen_width // 2 - ball_size // 2
            ball.y = screen_height // 2 - ball_size // 2
            ball_dx *= random.choice((-1, 1))
            ball_dy *= random.choice((-1, 1))

        screen.fill(black)
        draw_paddle(left_paddle)
        draw_paddle(right_paddle)
        draw_ball(ball)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    print(f"Final player score: {player_score}")
    sys.exit(player_score)

if __name__ == "__main__":
    main()
