#!/usr/bin/env python3
  
import rospy
import cv2
import os,rospkg
import numpy as np
from sensor_msgs.msg import Image, CompressedImage
from morai_msgs.msg import GPSMessage
from cv_bridge import CvBridgeError
from cv_bridge import CvBridge 
from slidewindow import SlideWindow
from morai_msgs.msg import CtrlCmd
import tf
from std_msgs.msg import Float64,Int16,Float32MultiArray
from nav_msgs.msg import Path,Odometry

class IMGParser:
    def __init__(self):
    
        self.image_sub = rospy.Subscriber("/image_jpeg/compressed", CompressedImage, self.callback)
        self.gps_sub = rospy.Subscriber("/gps", GPSMessage, self.navsat_callback)
        self.odom_sub = rospy.Subscriber("/odom", Odometry, self.odom_callback)
        self.initialized = False
        self.bridge = CvBridge()
        self.slidewindow = SlideWindow()
        self.camera_pub= rospy.Publisher('/camera_steering', CtrlCmd, queue_size=1)
        self.ctrl_msg = CtrlCmd()
        self.steering_angle = 0.0
        self.last_steering=0.0
        self.error=0
        self.num = 0
        self.lat=0
        self.lon=0
        self.odom_msg = Odometry()
        # self.crop_pts = np.array(
        #     [[
        #     [0,420],
        #     [210,310],
        #     [490,310],
        #     [640,400]
        #     ]]
        # )
        rospy.spin()
    def callback(self, msg):
        if self.lat == 0 and self.lon ==0 and self.num==1:
            try:
                img_bgr = self.bridge.compressed_imgmsg_to_cv2(msg)
            except CvBridgeError as e:
                print(e)

            # img_hsv = cv2.cvtColor(img_bgr,cv2.COLOR_BGR2HSV)
            # lower_wlane = np.array([20,70,70])
            # upper_wlane = np.array([40,255,255])

            # img_wlane = cv2.inRange(img_hsv, lower_wlane, upper_wlane)
            # img_wlane = cv2.cvtColor(img_wlane,cv2.COLOR_GRAY2BGR)
            # img_concat = np.concatenate([img_bgr,img_hsv, img_wlane],axis=1)

            # self.mask = self.mask_roi(img_bgr)

            # if len(self.mask.shape)==3:
            #     img_concat = np.concatenate([img_bgr, self.mask],axis=1)
            # else:
            #     img_concat = np.concatenate([img_bgr,cv2.cvtColor(self.mask, cv2.COLOR_GRAY2BGR)],axis=1)
            img_warp = self.warp_image(img_bgr)

            # if self.initialized == False:
            #     cv2.namedWindow("Simulator_Image", cv2.WINDOW_NORMAL) 
            #     cv2.createTrackbar('low_H', 'Simulator_Image', 50, 255, self.nothing)
            #     cv2.createTrackbar('low_S', 'Simulator_Image', 50, 255, self.nothing)
            #     cv2.createTrackbar('low_V', 'Simulator_Image', 50, 255, self.nothing)
            #     cv2.createTrackbar('high_H', 'Simulator_Image', 255, 255, self.nothing)
            #     cv2.createTrackbar('high_S', 'Simulator_Image', 255, 255, self.nothing)
            #     cv2.createTrackbar('high_V', 'Simulator_Image', 255, 255, self.nothing)
            #     self.initialized = True

            # cv2.imshow("Image window",img_warp)

            # low_H = cv2.getTrackbarPos('low_H', 'Simulator_Image')
            # low_S = cv2.getTrackbarPos('low_S', 'Simulator_Image')
            # low_V = cv2.getTrackbarPos('low_V', 'Simulator_Image')
            # high_H = cv2.getTrackbarPos('high_H', 'Simulator_Image')
            # high_S = cv2.getTrackbarPos('high_S', 'Simulator_Image')
            # high_V = cv2.getTrackbarPos('high_V', 'Simulator_Image')


            # cv2.cvtColor(img_warp, cv2.COLOR_BGR2HSV) # BGR to HSV
            #흰색
            lower_lane = np.array([142,149,156]) 
            upper_lane = np.array([188,184,208])
            #노란색
            # lower_lane = np.array([0,134,214]) 
            # upper_lane = np.array([255,191,255])
            #회색에 가까움
            #lower_lane = np.array([145,154,160]) 
            #upper_lane = np.array([160,211,199])
            # lower_lane = np.array([low_H, low_S, low_V]) # 
            # upper_lane = np.array([high_H, high_S, high_V])

            lane_image = cv2.inRange(img_warp, lower_lane, upper_lane)


            #lane_image = cv2.inRange(img_warp, lower_lane, upper_lane)

            #cv2.imshow("Lane Image", lane_image)
            self.lane_detection(lane_image)

            #cv2.waitKey(1)
        if self.odom_msg.pose.pose.position.x>=402456.5 and self.odom_msg.pose.pose.position.x<=402458.2 and self.odom_msg.pose.pose.position.y>=4133026.2 and self.odom_msg.pose.pose.position.y<=4133029:
            self.num=1

    def lane_detection(self, lane_image) :
        kernel_size = 5
        lane_image= cv2.GaussianBlur(lane_image,(kernel_size, kernel_size), 0)
        #warped_img = self.warper.warp(blur_img)
        #cv2.imshow("warped_img", lane_image)
        self.slide_img, self.slide_x_location, self.current_lane_window = self.slidewindow.slidewindow(lane_image)
        print('S자 mission slide---------------',self.slide_x_location,)
        if self.slide_x_location < 0 or self.slide_x_location>640:
            self.slide_x_location = 420
            self.error=1
        #cv2.imshow("slide_img", self.slide_img)
        if self.slide_x_location!=0:
            print(self.error)
            if self.error==1 and self.slide_x_location<320:
                self.slide_x_location = 420
            self.steering_angle = (self.slide_x_location - 320)*0.003*-1
            self.last_steering=self.steering_angle
        
        if self.slide_x_location==0:
            self.steering_angle=self.last_steering
        
        self.ctrl_msg.steering = self.steering_angle
        self.ctrl_msg.velocity = 8.0
        self.ctrl_msg.longlCmdType=2
        self.camera_pub.publish(self.ctrl_msg)


        
        # print(steering_angle)
        #rospy.loginfo("CURRENT LANE WINDOW: {}".format(self.current_lane_window))

    # def mask_roi(self, img):
    #     h = img.shape[0]
    #     w = img.shape[1]

    #     if len(img.shape)==3:
    #         c = img.shape[2]
    #         mask = np.zeros((h,w,c),dtype=np.uint8)

    #         mask_value = (255,255,255)
    #     else:

    #         mask = np.zeros((h,w),dtype=np.uint8)

    #         mask_value = (255)
        
    #     cv2.fillPoly(mask, self.crop_pts, mask_value)

    #     mask=cv2.bitwise_and(mask, img)

    #     return mask
    
    def warp_image(self,img):
        image_size = (img.shape[1],img.shape[0])

        H = 480 #480
        W = 640#640
        source_points= np.float32([[50, 350], [640, 350], [50, 255], [640, 255]])#좌하, 우하, 좌상, 우상
        destination_points = np.float32([[50, H-100], [W, H-100], [50, 0], [W, 0]])

        perspective_transform = cv2.getPerspectiveTransform(source_points,destination_points)
        warped_img = cv2.warpPerspective(img,perspective_transform, image_size, flags=cv2.INTER_LINEAR)
        
        return warped_img
    
    def lane_callback(self) :
        
        self.ctrl_msg.steering = self.steering_angle
        self.ctrl_msg.velocity = 6.0
        self.camera_pub.publish(self.ctrl_msg)
    
    def nothing(self,x):
        pass
    
    def navsat_callback(self, gps_msg):
        self.lat = gps_msg.latitude
        self.lon = gps_msg.longitude
    
    def odom_callback(self,data):
        self.odom_msg=data
      

if __name__ == '__main__':
    rospy.init_node('image_parser',anonymous=True)

    image_parser = IMGParser()

    rospy.spin()