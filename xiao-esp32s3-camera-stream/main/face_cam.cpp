#include "face_cam.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "dl_image_jpeg.hpp"
#include "human_face_detect.hpp"

#include "cam_capture.h"

static const char *TAG = "face_cam";

static SemaphoreHandle_t s_mutex;
static HumanFaceDetect *s_detect;

extern "C" esp_err_t face_cam_init(void)
{
    if (s_detect) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = cam_capture_init();
    if (err != ESP_OK) {
        return err;
    }

    // Model itself is lazy-loaded on the first run() call.
    s_detect = new (std::nothrow) HumanFaceDetect();
    if (!s_detect) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "face detection ready");
    return ESP_OK;
}

extern "C" esp_err_t face_cam_capture(face_cam_frame_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!s_detect) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t *jpeg = NULL;
    size_t jpeg_len = 0;
    esp_err_t err = cam_capture_jpeg(&jpeg, &jpeg_len, &out->width, &out->height);
    if (err != ESP_OK) {
        return err;
    }

    dl::image::jpeg_img_t jpeg_img = {
        .data = jpeg,
        .data_len = jpeg_len,
    };
    dl::image::img_t img = dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    if (!img.data) {
        ESP_LOGE(TAG, "jpeg decode failed");
        heap_caps_free(jpeg);
        memset(out, 0, sizeof(*out));
        return ESP_FAIL;
    }

    // The detector keeps internal state across run() calls.
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    auto &results = s_detect->run(img);
    heap_caps_free(img.data);

    for (const auto &res : results) {
        if (out->num_faces >= FACE_CAM_MAX_FACES) {
            ESP_LOGW(TAG, "dropping detections beyond %d faces", FACE_CAM_MAX_FACES);
            break;
        }
        face_info_t *f = &out->faces[out->num_faces++];
        f->score = res.score;
        f->x1 = res.box[0];
        f->y1 = res.box[1];
        f->x2 = res.box[2];
        f->y2 = res.box[3];
        // MSR+MNP keypoints: left_eye, mouth_left, nose, right_eye, mouth_right
        if (res.keypoint.size() >= 10) {
            f->has_keypoints = true;
            f->left_eye = {res.keypoint[0], res.keypoint[1]};
            f->mouth_left = {res.keypoint[2], res.keypoint[3]};
            f->nose = {res.keypoint[4], res.keypoint[5]};
            f->right_eye = {res.keypoint[6], res.keypoint[7]};
            f->mouth_right = {res.keypoint[8], res.keypoint[9]};
        }
    }
    xSemaphoreGive(s_mutex);

    out->jpeg = jpeg;
    out->jpeg_len = jpeg_len;
    return ESP_OK;
}

extern "C" void face_cam_frame_free(face_cam_frame_t *out)
{
    if (!out) {
        return;
    }
    heap_caps_free(out->jpeg);
    memset(out, 0, sizeof(*out));
}
