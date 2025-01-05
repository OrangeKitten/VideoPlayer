#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "Sample format %s not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return AVERROR(EINVAL);
}

/**
 * 利用SwrContext结构体进行音频重采样
 * @param argc 命令行参数的数量
 * @param argv 命令行参数的数组
 * @return 返回程序运行的状态，0表示成功，-1表示失败
 */
int main(int argc, char **argv)
{
    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO, dst_ch_layout = AV_CH_LAYOUT_SURROUND;
    int src_rate = 48000, dst_rate = 44100;
    uint8_t **src_data = NULL, **dst_data = NULL;
    int src_nb_channels = 0, dst_nb_channels = 0;
    int src_linesize, dst_linesize;
    int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_DBL, dst_sample_fmt = AV_SAMPLE_FMT_S16;
    const char *dst_filename = NULL;
    FILE *dst_file;
    int dst_bufsize;
    const char *fmt;
    struct SwrContext *swr_ctx;
    double t;
    int ret;

    if (argc!= 2) {
        fprintf(stderr, "Usage: %s output_file\n"
                "API example program to show how to resample an audio stream with libswresample.\n"
                "This program generates a series of audio frames, resamples them to a specified "
                "output format and rate and saves them to an output file named output_file.\n",
            argv[0]);
        exit(1);
    }
    dst_filename = argv[1];

    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }

    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        goto end;
    }

    /* allocate source and destination samples buffers */

    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto end;
    }

    /* compute the number of converted samples: buffering is avoided
    dst_nb_samples = av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
    max_dst_nb_samples = dst_nb_samples; */

    /* allocate destination samples buffer */

  // 根据计算出的最大目标样本数，为目标样本数据分配内存，并返回一个指针数组 dst_data，以及每行数据的大小 dst_linesize
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             max_dst_nb_samples, dst_sample_fmt, 0);
  // 如果内存分配失败，打印错误信息，并跳转到 end 标签，释放资源并退出程序
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        goto end;
    }

    /* generate synthetic audio data */

    // 初始化时间变量 t 为0
    t = 0;
    // 调用 fill_samples 函数生成音频数据
    fill_samples(src_data[0], src_nb_samples, src_nb_channels, src_rate, &t);

    /* resample */

    // 将转换后的样本数初始化为 0
    dst_nb_samples = 0;
    // 当源音频数据缓冲区（src_data）中还有样本时，循环执行重采样操作
    while (src_nb_samples > 0) {
      // 调用 swr_convert 函数进行音频重采样
        ret = swr_convert(swr_ctx, dst_data, max_dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
      // 如果重采样过程中发生错误，打印错误信息，并跳转到 end 标签
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            goto end;
        }
      // 获取实际转换后的样本数
        dst_nb_samples = ret;
      // 源音频数据缓冲区的样本数减少实际转换后的样本数
        src_nb_samples -= ret;
      // 移动源音频数据缓冲区的指针，指向下一个未处理的样本位置
        src_data += ret * src_nb_channels;
      // 释放之前分配的源音频数据缓冲区内存
        av_freep(&src_data);
      // 重新分配源音频数据缓冲区内存，以包含新的样本数据
        ret = av_samples_alloc(src_data, &src_linesize, src_nb_channels,
                               dst_nb_samples, src_sample_fmt, 1);
      // 如果内存分配失败，打印错误信息，并跳转到 end 标签
        if (ret < 0) {
            fprintf(stderr, "Could not reallocate source samples\n");
            goto end;
        }
      // 移动目标音频数据缓冲区的指针，指向下一个未写入的样本位置
        dst_data += dst_nb_samples * dst_nb_channels;
      // 计算当前目标音频数据缓冲区中还能容纳的最大样本数
        max_dst_nb_samples -= dst_nb_samples;
    }

    /* write the resampled audio data to the output file */

    // 根据目标样本格式获取数据格式字符串
    fmt = av_get_sample_fmt_name(dst_sample_fmt);
    // 如果获取格式字符串失败，打印错误信息，并跳转到 end 标签
    if (!fmt) {
        fprintf(stderr, "Could not get sample format string\n");
        goto end;
    }
    // 打印一条消息，显示正在写入输出文件
    printf("Writing data to file %s\n", dst_filename);
    // 计算目标音频数据缓冲区的数据大小，以字节为单位
    dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, dst_nb_samples, dst_sample_fmt, 1);
    // 将目标音频数据缓冲区的数据写入到输出文件中
    fwrite(dst_data[0], 1, dst_bufsize, dst_file);

    /* free the allocated memory */

    // 关闭输出文件
    fclose(dst_file);
    // 释放 SwrContext 上下文结构体
    swr_free(&swr_ctx);
    // 释放目标音频数据缓冲区内存
    av_freep(&dst_data);

    return 0;

end:
    if (dst_file)
        fclose(dst_file);
    if (swr_ctx)    
        swr_free(&swr_ctx);
    if (src_data)   
        av_freep(&src_data);
    if (dst_data)   
        av_freep(&dst_data);

    return ret;
}
