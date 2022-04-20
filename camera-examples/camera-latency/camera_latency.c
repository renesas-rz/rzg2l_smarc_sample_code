#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <signal.h>

#define CAM_DEVICE	"/dev/video0"
#define DISP_DEVICE	"/dev/fb0"

#define CAM_WIDTH	1920
#define CAM_HEIGHT	1080
#define CAM_PIXSIZE	4
#define CAM_FRAMESIZE	(CAM_WIDTH * CAM_HEIGHT * CAM_PIXSIZE)

#define DISP_WIDTH	1920
#define DISP_HEIGHT	1080
#define DISP_PIXSIZE	4
#define DISP_FRAMESIZE	(DISP_WIDTH * DISP_HEIGHT * DISP_PIXSIZE)

#define NB_DISP_BUFFER	1
#define NB_CAM_BUFFER	4

static unsigned long cam_buf[NB_CAM_BUFFER];
static u_int8_t *disp_buf;
static int cur_cam_idx = 0;
static int cam_fd;
static int disp_fd;
struct fb_var_screeninfo info_bk;
struct fb_fix_screeninfo fixinfo;

static int debug = 0;
#define debug(fmt, args...) if (debug) printf("[DBG]: " fmt, args)

#include <sys/time.h>
struct timeval tv_start;
struct timeval tv_stop;
int frame_count = 0;

static int release_framedata(void)
{
    struct v4l2_buffer v4lb;
    int ret;

    memset(&v4lb, 0, sizeof(struct v4l2_buffer));
    v4lb.index = cur_cam_idx;
    debug(" -*- Q:index: %d\n", v4lb.index);
    v4lb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4lb.memory = V4L2_MEMORY_USERPTR;
    v4lb.m.userptr = cam_buf[cur_cam_idx];
    v4lb.length = CAM_FRAMESIZE;

    ret = ioctl(cam_fd, VIDIOC_QBUF, &v4lb);
    if (ret < 0) {
        perror("Unable to requeue buffer");
        return -1;
    }

    return 0;
}

static int get_framedata(void)
{
    struct v4l2_buffer v4lb;
    int ret;

    memset(&v4lb, 0, sizeof(struct v4l2_buffer));
    v4lb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4lb.index = cur_cam_idx;
    v4lb.memory = V4L2_MEMORY_USERPTR;
    ret = ioctl(cam_fd, VIDIOC_DQBUF, &v4lb);
    if (ret < 0) {
        perror("Unable to dequeue buffer");
        return -1;
    }
    debug(" -*- DQ:index: %d\n", v4lb.index);
    frame_count++;

    return 0;
}

static int process_main(void)
{
    get_framedata();

#if NB_DISP_BUFFER > 1
    {
        struct fb_var_screeninfo info;
        static int toggle = 1;
        int ret;

        ret = ioctl(disp_fd, FBIOGET_VSCREENINFO, &info);
        if (ret < 0)
            perror("FBIOGET_VSCREENINFO");

        if (toggle++) {
            info.yoffset = 0;
            toggle = 0;
        } else {
            info.yoffset = DISP_HEIGHT;
        }

        ret = ioctl(disp_fd, FBIOPUT_VSCREENINFO, &info);
        if (ret < 0)
            perror("FBIOPUT_VSCREENINFO");
        ret = ioctl(disp_fd, FBIOPAN_DISPLAY, &info);
        if (ret < 0)
            perror("FBIOPAN_DISPLAY");
    }
#endif
    release_framedata();
    return 0;
}

void exitfunc(void)
{
    int fd;
    float duration;
    float frame_rate;

    gettimeofday(&tv_stop, NULL);
    duration = (float)(tv_stop.tv_sec - tv_start.tv_sec) + (float)(tv_stop.tv_usec - tv_start.tv_usec)/1000000;
    frame_rate = (float)frame_count/duration;

    printf("\n");
    printf("Number of frames : %d\n",frame_count);
    printf("Playing duration : %f s\n",duration);
    printf("Framerate        : %f fps\n",frame_rate);

    fd = open(DISP_DEVICE, O_RDWR);
    ioctl(fd,FBIOPUT_VSCREENINFO,&info_bk);
    close(fd);
}

void sighandler(int sig)
{
    exit(0);
}

int main(int argc, char **argv)
{
    int ret;
    int i;


    atexit(exitfunc);
    signal(SIGINT, sighandler);

    disp_fd = open(DISP_DEVICE, O_RDWR);
    ret = ioctl (disp_fd, FBIOGET_FSCREENINFO, &fixinfo);
    if (ret < 0)
        perror("FBIOGET_FSCREENINFO");

    ret = ioctl(disp_fd,FBIOGET_VSCREENINFO,&info_bk);
    if (ret < 0)
        perror("FBIOPUT_VSCREENINFO");
            
    disp_buf = mmap(0, DISP_FRAMESIZE * NB_DISP_BUFFER,
            PROT_WRITE, MAP_SHARED,
            disp_fd, 0);

    if (disp_buf == (void *)-1) {
        perror("mmap");
        return -EINVAL;
    }
    memset(disp_buf, 0x00, DISP_FRAMESIZE * NB_DISP_BUFFER);


    cam_fd = open(CAM_DEVICE, O_RDWR);
    if (cam_fd == -1) {
        perror("camera:open");
        return -1;
    }

    {
        struct v4l2_format fmt;
        struct v4l2_streamparm parm;
        struct v4l2_format fmt1;

        memset(&fmt, 0, sizeof(struct v4l2_format));
        memset(&fmt1, 0, sizeof(struct v4l2_format));
        memset(&parm, 0, sizeof(struct v4l2_streamparm));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // fmt.fmt.pix.sizeimage = CAM_FRAMESIZE;
        fmt.fmt.pix.width = CAM_WIDTH;
        fmt.fmt.pix.height = CAM_HEIGHT;
        fmt.fmt.pix.pixelformat =  V4L2_PIX_FMT_XBGR32;
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        ret = ioctl(cam_fd, VIDIOC_G_FMT, &fmt);

        if (ret == -1) {
            perror("camera:ioctl:VIDIOC_G_FMT");
            return -EIO;
        }

        ret = ioctl(cam_fd, VIDIOC_TRY_FMT, &fmt);
        if (ret == -1) {
            perror("camera:ioctl:VIDIOC_TRY_FMT");
            return -EIO;
        }
        ret = ioctl(cam_fd, VIDIOC_S_FMT, &fmt);
        if (ret == -1) {
            perror("camera:ioctl:VIDIOC_S_FMT");
            return -EIO;
        }
    }

    {
        struct v4l2_requestbuffers v4lrb;
        struct v4l2_buffer v4lb;
        int idx;

        memset(&v4lrb, 0, sizeof(struct v4l2_requestbuffers));
        v4lrb.count = NB_CAM_BUFFER;
        v4lrb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4lrb.memory = V4L2_MEMORY_USERPTR;
        ret = ioctl(cam_fd, VIDIOC_REQBUFS, &v4lrb);
        if (ret < 0) {
            perror("Unable to allocate buffers");
            return -1;
        }

        for (i=0; i<NB_CAM_BUFFER; i++) {
            idx = i % NB_DISP_BUFFER;
            memset(&v4lb, 0, sizeof(struct v4l2_buffer));
            v4lb.index = i;
            v4lb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            v4lb.memory = V4L2_MEMORY_USERPTR;
            v4lb.length = CAM_FRAMESIZE;
            v4lb.m.userptr = (unsigned long)
                &disp_buf[v4lb.length * idx];

            cam_buf[i] = v4lb.m.userptr;

            ret = ioctl(cam_fd, VIDIOC_QBUF, &v4lb);
            if (ret < 0) {
                perror("Unable to queue buffer");
                return -1;
            }
        }
    }

    {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(cam_fd, VIDIOC_STREAMON, &type);
        if (ret < 0) {
          perror("Unable to start capture");
          return -1;
        }
    }

    frame_count = 0;
    gettimeofday(&tv_start, NULL);
    while (1) {
        process_main();
        cur_cam_idx++;
        if (cur_cam_idx == NB_CAM_BUFFER)
            cur_cam_idx = 0;
    };

    munmap(disp_buf, DISP_FRAMESIZE * NB_DISP_BUFFER);
    close(disp_fd);
    close(cam_fd);

    return 0;
}
