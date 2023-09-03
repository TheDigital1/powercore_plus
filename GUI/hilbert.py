'''
Hilbert curve to gcode

This is based on Python Turtle code by ElectroNerd posted here:
https://stackoverflow.com/questions/7218269/hilbert-curve-analysis

Gcode added by Mike Szczys for use on the midTbot
https://hackaday.io/project/169033-gcode-playground

Usage: Run "plotHilber()"
'''

import turtle
from turtle import left, right, forward
size = 10

#Not sure why we need a negative offset but we do
x = 3
y = 0
direction = 1
step = 1
xyfeed = 10
zfeed = 100

UP = 0
RT = 1
DN = 2
LT = 3
RIGHT_TURN = 1
LEFT_TURN = -1

def zeroPen():
    print("G0Z5 F%i" % zfeed)
    print("G0X0Y0 F%i" % xyfeed)

def printGmove(x,y):
    print("G0X%iY%i F%i" % (x,y,xyfeed))

def changeDir(val):
    global direction
    direction = (direction+val)%4

def movePen():
    global x,y
    if direction == 0:
        y += step
    elif direction == 1:
        x += step
    elif direction == 2:
        y -= step
    elif direction == 3:
        x -= step
    printGmove(x,y)

def hilbert(level, angle):
    global x, y
    if level == 0:
        return

    turtle.color("Blue")
    turtle.speed(10)

    right(angle)
    if angle > 0:
        changeDir(RIGHT_TURN)
    else:
        changeDir(LEFT_TURN)
        
    hilbert(level - 1, -angle)
    forward(size)
    movePen()
    left(angle)
    if angle < 0:
        changeDir(RIGHT_TURN)
    else:
        changeDir(LEFT_TURN)
    
    hilbert(level - 1, angle)
    forward(size)
    movePen()
    hilbert(level - 1, angle)
    left(angle)
    if angle < 0:
        changeDir(RIGHT_TURN)
    else:
        changeDir(LEFT_TURN)

    forward(size)
    movePen()
    hilbert(level - 1, -angle)
    right(angle)
    if angle > 0:
        changeDir(RIGHT_TURN)
    else:
        changeDir(LEFT_TURN)

def plotHilbert():
    global x,y
    #x=0
    #y=0
    print("G90")
    zeroPen()
    print("G0Z0 F%i" % zfeed)
    hilbert(5,90)
    zeroPen()

plotHilbert()