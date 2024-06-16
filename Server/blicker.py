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
red = (255, 0, 0)
blue = (0, 0, 255)

# Paddle dimensions
paddle_width = 100
paddle_height = 10

# Ball dimensions
ball_size = 10

# Brick dimensions
brick_width = 75
brick_height = 20
brick_rows = 5
brick_columns = 10

# Speeds
ball_speed_x = 1
ball_speed_y = -1
paddle_speed = 50

# Create the screen
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Brick Breaker")

# Clock for controlling the frame rate
clock = pygame.time.Clock()

def draw_paddle(paddle):
    pygame.draw.rect(screen, white, paddle)

def draw_ball(ball):
    pygame.draw.ellipse(screen, white, ball)

def draw_bricks(bricks):
    for brick in bricks:
        pygame.draw.rect(screen, red, brick)

# Global variable to track if the game should end
terminate_game = False

def read_messages():
    global terminate_game
    while True:
        message = sys.stdin.buffer.readline().rstrip()  # Read the message as bytes and remove trailing newline

        if "END" in message.decode('utf-8'):
            print("Received termination signal. Exiting...")
            terminate_game = True
            break
        elif "LEFT" in message.decode('utf-8'):
            move_paddle(-1)  # Move paddle left
        elif "RIGHT" in message.decode('utf-8'):
            move_paddle(1)   # Move paddle right

def move_paddle(direction):
    global paddle
    if direction == -1 and paddle.left > 0:
        paddle.x -= paddle_speed
    elif direction == 1 and paddle.right < screen_width:
        paddle.x += paddle_speed

def reset_game():
    global paddle, ball, ball_dx, ball_dy, bricks, player_score, terminate_game
    paddle = pygame.Rect(screen_width // 2 - paddle_width // 2, screen_height - 30, paddle_width, paddle_height)
    ball = pygame.Rect(screen_width // 2 - ball_size // 2, screen_height // 2 - ball_size // 2, ball_size, ball_size)

    ball_dx = ball_speed_x * random.choice((-1, 1))
    ball_dy = ball_speed_y

    bricks = []
    for row in range(brick_rows):
        for col in range(brick_columns):
            brick_x = col * (brick_width + 10) + 35
            brick_y = row * (brick_height + 10) + 35
            bricks.append(pygame.Rect(brick_x, brick_y, brick_width, brick_height))

    player_score = 0
    terminate_game = False

def main():
    global paddle, ball, ball_dx, ball_dy, bricks, player_score, terminate_game

    # Start a separate thread to read messages from stdin
    message_thread = threading.Thread(target=read_messages)
    message_thread.daemon = True
    message_thread.start()

    reset_game()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        if terminate_game:
            reset_game()

        # Check for input from stdin (if any)
        if sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
            message = sys.stdin.buffer.readline().rstrip()
            if message:
                decoded_message = message.decode('utf-8')
                if decoded_message == "END":
                    print("Received termination signal. Exiting...")
                    running = False
                elif decoded_message == "LEFT":
                    move_paddle(-1)
                elif decoded_message == "RIGHT":
                    move_paddle(1)

        ball.x += ball_dx
        ball.y += ball_dy

        if ball.left <= 0 or ball.right >= screen_width:
            ball_dx *= -1
        if ball.top <= 0:
            ball_dy *= -1
        if ball.colliderect(paddle):
            ball_dy *= -1

        bricks_hit = ball.collidelistall(bricks)
        if bricks_hit:
            for idx in bricks_hit:
                brick = bricks[idx]
                if ball.centerx < brick.left or ball.centerx > brick.right:
                    ball_dx *= -1
                else:
                    ball_dy *= -1
                bricks.pop(idx)
                player_score += 10
                break

        if ball.top >= screen_height:
            print("Missed the ball! Restarting game...")
            reset_game()

        screen.fill(black)
        draw_paddle(paddle)
        draw_ball(ball)
        draw_bricks(bricks)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    print(f"Final player score: {player_score}")
    sys.exit(player_score)

if __name__ == "__main__":
    main()
