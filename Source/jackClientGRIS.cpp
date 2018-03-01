/*
 This file is part of ServerGris.
 
 Developers: Nicolas Masson
 
 ServerGris is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 ServerGris is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with ServerGris.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include "jackClientGRIS.h"
#include "vbap.h"
#include "fft.h"
#include "Speaker.h"
#include "MainComponent.h"

static bool jack_client_log_print = false;

//=========================================================================================
//Utilities
//=========================================================================================
static bool int_vector_contains(vector<int> vec, int value) {
    return (std::find(vec.begin(), vec.end(), value) != vec.end());
}

static void jack_client_log(const char* format, ...) {
    if (jack_client_log_print) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        printf("%s", buffer);
    }
}

//=========================================================================================
//MUTE SOLO MasterGainOut and NOISE
//=========================================================================================
static void muteSoloVuMeterIn(jackClientGris & jackCli, jack_default_audio_sample_t ** ins,
                              const jack_nframes_t &nframes, const unsigned int &sizeInputs) {
    
    float sumsIn[sizeInputs];
    
    for (unsigned int i = 0; i < sizeInputs; ++i) {
        // Mute ----------------
        if (jackCli.listSourceIn[i].isMuted) {
            memset(ins[i], 0, sizeof(jack_default_audio_sample_t) * nframes);
        }
        // Solo ----------------
        else if (jackCli.soloIn) {
            if (!jackCli.listSourceIn[i].isSolo) {
                memset (ins[i], 0, sizeof(jack_default_audio_sample_t) * nframes);
            }
        }
        // Vu Meter ----------------
        sumsIn[i] = ins[i][0] * ins[i][0];
        for(unsigned int nF = 1; nF < nframes; ++nF) {
            sumsIn[i] +=  ins[i][nF] * ins[i][nF];
        }
        jackCli.levelsIn[i] = sqrtf(sumsIn[i] / nframes);
    }
}

static void muteSoloVuMeterGainOut(jackClientGris & jackCli, jack_default_audio_sample_t ** outs,
                                   const jack_nframes_t &nframes, const unsigned int &sizeOutputs,
                                   const float mGain = 1.0f) {
    unsigned int num_of_channels;
    float sumsOut[sizeOutputs];
    float gain;
    double inval = 0.0, val = 0.0;

    if (jackCli.modeSelected == VBap) {
        num_of_channels = sizeOutputs;
    } else if (jackCli.modeSelected == HRTF_LOW || jackCli.modeSelected == HRTF_HIGH || jackCli.modeSelected == STEREO) {
        num_of_channels = 2;
    }
    
    for (unsigned int i = 0; i < sizeOutputs; ++i) {
        // Mute ----------------
        if (jackCli.listSpeakerOut[i].isMuted) {
            memset(outs[i], 0, sizeof(jack_default_audio_sample_t) * nframes);
        }
        // Solo ----------------
        else if (jackCli.soloOut) {
            if (!jackCli.listSpeakerOut[i].isSolo) {
                memset(outs[i], 0, sizeof(jack_default_audio_sample_t) * nframes);
            }
        }
        // Speaker independent gain
        gain = jackCli.listSpeakerOut[i].gain;
        for (unsigned int f = 0; f < nframes; ++f) {
            outs[i][f] *= gain;
        }
        // Speaker independent crossover filter
        if (jackCli.listSpeakerOut[i].hpActive) {
            SpeakerOut so = jackCli.listSpeakerOut[i];
            for (unsigned int f = 0; f < nframes; ++f) {
                inval = (double)outs[i][f];
                val = so.ha0 * inval + so.ha1 * jackCli.x1[i] + so.ha2 * jackCli.x2[i] +
                      so.ha1 * jackCli.x3[i] + so.ha0 * jackCli.x4[i] - so.b1 * jackCli.y1[i] -
                      so.b2 * jackCli.y2[i] - so.b3 * jackCli.y3[i] - so.b4 * jackCli.y4[i];
                jackCli.y4[i] = jackCli.y3[i];
                jackCli.y3[i] = jackCli.y2[i];
                jackCli.y2[i] = jackCli.y1[i];
                jackCli.y1[i] = val;
                jackCli.x4[i] = jackCli.x3[i];
                jackCli.x3[i] = jackCli.x2[i];
                jackCli.x2[i] = jackCli.x1[i];
                jackCli.x1[i] = inval;
                outs[i][f] = (jack_default_audio_sample_t)val;
            }
        }
        // Vu Meter ----------------
        outs[i][0] *= mGain; // Master volume
        sumsOut[i] = outs[i][0] * outs[i][0];
        for (unsigned int nF = 1; nF < nframes; ++nF) {
            outs[i][nF] *= mGain;
            sumsOut[i] +=  outs[i][nF] * outs[i][nF];
        }
        jackCli.levelsOut[i] = sqrtf(sumsOut[i] / nframes);
        
        // Record buffer ---------------
        if (jackCli.recording) {
            if (num_of_channels == sizeOutputs && i < num_of_channels) {
                if (int_vector_contains(jackCli.outputPatches, i+1)) {
                    jackCli.recorder[i].recordSamples(&outs[i], (int)nframes);
                }
            } else if (num_of_channels == 2 && i < num_of_channels) {
                jackCli.recorder[i].recordSamples(&outs[i], (int)nframes);
            }
        }
    }
    
    // Record - Up index ----------
    if (!jackCli.recording && jackCli.indexRecord > 0) {
        if (num_of_channels == sizeOutputs) {
            for (unsigned int i = 0; i < sizeOutputs; ++i) {
                if (int_vector_contains(jackCli.outputPatches, i+1) && i < num_of_channels) {
                    jackCli.recorder[i].stop();
                }
            }
        } else if (num_of_channels == 2) {
            jackCli.recorder[0].stop();
            jackCli.recorder[1].stop();
        }
        jackCli.indexRecord = 0;
    } else if (jackCli.recording) {
        jackCli.indexRecord += nframes;
    }

}

static void addNoiseSound(jackClientGris & jackCli, jack_default_audio_sample_t ** outs,
                          const jack_nframes_t &nframes, const unsigned int &sizeOutputs) {
    float rnd, val;
    float fac = 1.0f / (RAND_MAX / 2.0f);
    for(unsigned int nF = 0; nF < nframes; ++nF) {
        rnd = rand() * fac - 1.0f;
        jackCli.c0 = jackCli.c0 * 0.99886f + rnd * 0.0555179f;
        jackCli.c1 = jackCli.c1 * 0.99332f + rnd * 0.0750759f;
        jackCli.c2 = jackCli.c2 * 0.96900f + rnd * 0.1538520f;
        jackCli.c3 = jackCli.c3 * 0.86650f + rnd * 0.3104856f;
        jackCli.c4 = jackCli.c4 * 0.55000f + rnd * 0.5329522f;
        jackCli.c5 = jackCli.c5 * -0.7616f - rnd * 0.0168980f;
        val = jackCli.c0 + jackCli.c1 + jackCli.c2 + jackCli.c3 + jackCli.c4 + jackCli.c5 + jackCli.c6 + rnd * 0.5362f;
        val *= 0.2f;
        val *= jackCli.pinkNoiseGain;
        jackCli.c6 = rnd * 0.115926f;

        for (unsigned int i = 0; i < sizeOutputs; i++) {
            outs[i][nF] += val;
        }
    }
}

//=========================================================================================
//VBAP
//=========================================================================================
static void processVBAP(jackClientGris & jackCli, jack_default_audio_sample_t ** ins, jack_default_audio_sample_t ** outs,
                        const jack_nframes_t &nframes, const unsigned int &sizeInputs, const unsigned int &sizeOutputs)
{
    unsigned int f, i, o, ilinear;
    float y, interpG, iogain = 0.0;

    if (jackCli.interMaster == 0.0) {
        ilinear = 1;
    } else {
        ilinear = 0;
        interpG = powf(jackCli.interMaster, 0.1) * 0.0099 + 0.99;
    }

    for (i = 0; i < sizeInputs; ++i) {
        if (jackCli.vbapSourcesToUpdate[i] == 1) {
            jackCli.updateSourceVbap(i);
            jackCli.vbapSourcesToUpdate[i] = 0;
        }
    }

    for (o = 0; o < sizeOutputs; ++o) {
        memset(outs[o], 0, sizeof(jack_default_audio_sample_t) * nframes);
        for (i = 0; i < sizeInputs; ++i) {
            if (!jackCli.listSourceIn[i].directOut) {
                iogain = jackCli.listSourceIn[i].paramVBap->gains[o];
                y = jackCli.listSourceIn[i].paramVBap->y[o];
                if (ilinear) {
                    interpG = (iogain - y) / nframes;
                    for (f = 0; f < nframes; ++f) {
                        y += interpG;
                        outs[o][f] += ins[i][f] * y;
                    }
                } else {
                    for (f = 0; f < nframes; ++f) {
                        y = iogain + (y - iogain) * interpG;
                        if (y < 0.0000000000001f) {
                            y = 0.0;
                        } else {
                            outs[o][f] += ins[i][f] * y;
                        }
                    }
                }
                jackCli.listSourceIn[i].paramVBap->y[o] = y;
            } else if ((unsigned int)(jackCli.listSourceIn[i].directOut - 1) == o) {
                for (f = 0; f < nframes; ++f) {
                    outs[o][f] += ins[i][f];
                }
            }
        }
    }
}

//=========================================================================================
//HRTF
//=========================================================================================
static void 
processHRTF(jackClientGris & jackCli, jack_default_audio_sample_t ** ins, jack_default_audio_sample_t ** outs,
            const jack_nframes_t &nframes, const unsigned int &sizeInputs, const unsigned int &sizeOutputs) {
    unsigned int f, i, k, o, hsize = 64;
    int tmp_count, azim_index_down, azim_index_up, elev_index, elev_index_array;
    float azi, ele, norm_elev, sig, elev_frac, elev_frac_inv;
    float azim_frac_down, azim_frac_inv_down, azim_frac_up, azim_frac_inv_up, cross_coeff, cross_coeff_inv;
    float magL, angL, magR, angR;
    float inframeL[128], inframeR[128];
    float realL[64], imagL[64], realR[64], imagR[64];

    for (o = 0; o < sizeOutputs; ++o) {
        memset(outs[o], 0, sizeof(jack_default_audio_sample_t) * nframes);
    }

    for (i = 0; i < sizeInputs; ++i) {
        if (jackCli.listSourceIn[i].directOut == 0) {
            azi = jackCli.listSourceIn[i].azimuth;
            ele = jackCli.listSourceIn[i].zenith;

            if (azi < 0.0f) {
                azi += 360.0f;
            }
            if (azi >= 359.9999f) {
                azi = 359.9999f;
            }

            if (ele < -39.9999f) { 
                ele = -39.9999f;
            } else if (ele >= 89.9999f) {
                ele = 89.9999f;
            }

            for (f = 0; f < nframes; ++f) {
                if (jackCli.hrtf_sample_count == 0) {
                    /* Removes the chirp at 360->0 degrees azimuth boundary. */
                    if (abs(jackCli.hrtf_last_azi[i] - azi) > 300.0f) {
                        jackCli.hrtf_last_azi[i] = azi;
                    }

                    jackCli.hrtf_last_azi[i] = azi + (jackCli.hrtf_last_azi[i] - azi) * 0.5;
                    jackCli.hrtf_last_ele[i] = ele + (jackCli.hrtf_last_ele[i] - ele) * 0.5;

                    for (k=0; k<jackCli.HrtfImpulseLength; k++) {
                        jackCli.previous_impulses[i][0][k] = jackCli.current_impulses[i][0][k];
                        jackCli.previous_impulses[i][1][k] = jackCli.current_impulses[i][1][k];
                    }

                    norm_elev = jackCli.hrtf_last_ele[i] * 0.1f;
                    elev_index = (int)floor(norm_elev);
                    elev_index_array = elev_index + 4;
                    elev_frac = norm_elev - elev_index;
                    elev_frac_inv = 1.0f - elev_frac;

                    // If elevation is less than 80 degrees.
                    if (norm_elev < 8.0f) {
                        azim_index_down = (int)(jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array]);
                        azim_frac_down = (jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array]) - azim_index_down;
                        azim_frac_inv_down = 1.0f - azim_frac_down;
                        azim_index_up = (int)(jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array+1]);
                        azim_frac_up = (jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array+1]) - azim_index_up;
                        azim_frac_inv_up = 1.0f - azim_frac_up;
                        for (k=0; k<hsize; k++) {
                            magL = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.mag_left[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.mag_left[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac *
                                                (azim_frac_inv_up * jackCli.mag_left[elev_index_array+1][azim_index_up][k] +
                                                azim_frac_up * jackCli.mag_left[elev_index_array+1][azim_index_up+1][k]);
                            angL = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.ang_left[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.ang_left[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac *
                                                (azim_frac_inv_up * jackCli.ang_left[elev_index_array+1][azim_index_up][k] +
                                                azim_frac_up * jackCli.ang_left[elev_index_array+1][azim_index_up+1][k]);
                            magR = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.mag_right[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.mag_right[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac *
                                                (azim_frac_inv_up * jackCli.mag_right[elev_index_array+1][azim_index_up][k] +
                                                azim_frac_up * jackCli.mag_right[elev_index_array+1][azim_index_up+1][k]);
                            angR = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.ang_right[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.ang_right[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac *
                                                (azim_frac_inv_up * jackCli.ang_right[elev_index_array+1][azim_index_up][k] +
                                                azim_frac_up * jackCli.ang_right[elev_index_array+1][azim_index_up+1][k]);
                            realL[k] = magL * cosf(angL);
                            imagL[k] = magL * sinf(angL);
                            realR[k] = magR * cosf(angR);
                            imagR[k] = magR * sinf(angR);
                       }
                    // if elevation is 80 degrees or more, interpolation requires only three points (there's only one HRIR at 90 deg).
                    } else {
                        azim_index_down = (int)(jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array]);
                        azim_frac_down = (jackCli.hrtf_last_azi[i] / jackCli.hrtf_diff[elev_index_array]) - azim_index_down;
                        azim_frac_inv_down = 1.0f - azim_frac_down;
                        for (k=0; k<hsize; k++) {
                            magL = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.mag_left[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.mag_left[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac * jackCli.mag_left[13][0][k];
                            angL = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.ang_left[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.ang_left[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac * jackCli.ang_left[13][0][k];
                            magR = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.mag_right[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.mag_right[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac * jackCli.mag_right[13][0][k];
                            angR = elev_frac_inv *
                                                (azim_frac_inv_down * jackCli.ang_right[elev_index_array][azim_index_down][k] +
                                                azim_frac_down * jackCli.ang_right[elev_index_array][azim_index_down+1][k]) +
                                                elev_frac * jackCli.ang_right[13][0][k];
                            realL[k] = magL * cosf(angL);
                            imagL[k] = magL * sinf(angL);
                            realR[k] = magR * cosf(angR);
                            imagR[k] = magR * sinf(angR);
                        }
                    }
                    inframeL[0] = realL[0];
                    inframeR[0] = realR[0];
                    inframeL[hsize] = 0.0;
                    inframeR[hsize] = 0.0;
                    for (k=1; k<hsize; k++) {
                        inframeL[k] = realL[k];
                        inframeL[128 - k] = imagL[k];
                        inframeR[k] = realR[k];
                        inframeR[128 - k] = imagR[k];
                    }
                    irealfft_split(inframeL, jackCli.current_impulses[i][0], 128, jackCli.twiddle);
                    irealfft_split(inframeR, jackCli.current_impulses[i][1], 128, jackCli.twiddle);
                }
                tmp_count = jackCli.hrtf_count[i];
                cross_coeff = (float)jackCli.hrtf_sample_count / (float)jackCli.HrtfImpulseLength;
                cross_coeff_inv = 1.0f - cross_coeff;
                for (k=0; k<jackCli.HrtfImpulseLength; ++k) {
                    if (tmp_count < 0) {
                        tmp_count += jackCli.HrtfImpulseLength;
                    }
                    sig = jackCli.hrtf_input_tmp[i][tmp_count];
                    outs[0][f] += sig * (cross_coeff * jackCli.current_impulses[i][0][k] + cross_coeff_inv * jackCli.previous_impulses[i][0][k]);
                    outs[1][f] += sig * (cross_coeff * jackCli.current_impulses[i][1][k] + cross_coeff_inv * jackCli.previous_impulses[i][1][k]);
                    tmp_count--;
                }
                jackCli.hrtf_count[i]++;
                if (jackCli.hrtf_count[i] >= jackCli.HrtfImpulseLength) {
                    jackCli.hrtf_count[i] = 0;
                }
                jackCli.hrtf_input_tmp[i][jackCli.hrtf_count[i]] = ins[i][f];

                jackCli.hrtf_sample_count += 1;
                if (jackCli.hrtf_sample_count >= jackCli.HrtfImpulseLength) {
                    jackCli.hrtf_sample_count = 0;
                }
            }
        } else if ((jackCli.listSourceIn[i].directOut) == 1) {
            for (f = 0; f < nframes; ++f) {
                outs[0][f] += ins[i][f];
            }
        } else if ((jackCli.listSourceIn[i].directOut) == 2) {
            for (f = 0; f < nframes; ++f) {
                outs[1][f] += ins[i][f];
            }
        }
    }
}

//=========================================================================================
// STEREO
//=========================================================================================
static void processSTEREO(jackClientGris & jackCli, jack_default_audio_sample_t ** ins, jack_default_audio_sample_t ** outs,
                        const jack_nframes_t &nframes, const unsigned int &sizeInputs, const unsigned int &sizeOutputs)
{
    unsigned int f, i;
    float azi, last_azi, scaled;
    float factor = M_PI2 / 180.0f;
    float interpG = powf(jackCli.interMaster, 0.1) * 0.0099 + 0.99;
    float gain = powf(10.0f, (sizeInputs - 1) * -0.1f * 0.05f);

    for (i = 0; i < sizeOutputs; ++i) {
        memset(outs[i], 0, sizeof(jack_default_audio_sample_t) * nframes);
    }

    for (i = 0; i < sizeInputs; ++i) {
        if (!jackCli.listSourceIn[i].directOut) {
            azi = jackCli.listSourceIn[i].azimuth;
            last_azi = jackCli.last_azi[i];
            for (f = 0; f < nframes; ++f) {
                /* Removes the chirp at 180->-180 degrees azimuth boundary. */
                if (abs(last_azi - azi) > 300.0f) {
                    last_azi = azi;
                }
                last_azi = azi + (last_azi - azi) * interpG;
                if (last_azi < -90.0f) {
                    scaled = -90.0f - (last_azi + 90.0f);
                } else if (last_azi > 90) {
                    scaled = 90.0f - (last_azi - 90.0f);
                } else {
                    scaled = last_azi;
                }
                scaled = (scaled + 90) * factor;
                outs[0][f] += ins[i][f] * cosf(scaled);
                outs[1][f] += ins[i][f] * sinf(scaled);
            }
            jackCli.last_azi[i] = last_azi;

        } else if ((jackCli.listSourceIn[i].directOut) == 1) {
            for (f = 0; f < nframes; ++f) {
                outs[0][f] += ins[i][f];
            }
        } else if ((jackCli.listSourceIn[i].directOut) == 2) {
            for (f = 0; f < nframes; ++f) {
                outs[1][f] += ins[i][f];
            }
        }
    }
    /* Apply gain compensation. */
    for (f = 0; f < nframes; ++f) {
        outs[0][f] *= gain;
        outs[1][f] *= gain;
    }
}

//=========================================================================================
//MASTER PROCESS
//=========================================================================================
static int process_audio (jack_nframes_t nframes, void* arg) {

    jackClientGris* jackCli = (jackClientGris*)arg;
    
    //================ Return if user edit speaker ==============================
    if (!jackCli->processBlockOn) {
        for (unsigned int i = 0; i < jackCli->outputsPort.size(); ++i) {
            memset(((jack_default_audio_sample_t*)jack_port_get_buffer(jackCli->outputsPort[i], nframes)),
                   0, sizeof(jack_default_audio_sample_t) * nframes);
            jackCli->levelsOut[i] = 0.0f;
        }
        return 0;
    }
    
    //================ LOAD BUFFER ============================================
    const unsigned int sizeInputs = (unsigned int)jackCli->inputsPort.size();
    const unsigned int sizeOutputs = (unsigned int)jackCli->outputsPort.size();
    
    // Get all buffer from all input - output
    jack_default_audio_sample_t * ins[sizeInputs];
    jack_default_audio_sample_t * outs[sizeOutputs];
    
    for (unsigned int i = 0; i < sizeInputs; i++) {
        ins[i] = (jack_default_audio_sample_t *)jack_port_get_buffer(jackCli->inputsPort[i], nframes);
    }
    for (unsigned int i = 0; i < sizeOutputs; i++) {
        outs[i] = (jack_default_audio_sample_t *)jack_port_get_buffer(jackCli->outputsPort[i], nframes);
    }

    //================ INPUTS ===============================================
    muteSoloVuMeterIn(*jackCli, ins, nframes, sizeInputs);

    //================ PROCESS ==============================================    
    switch ((ModeSpatEnum)jackCli->modeSelected) {
        case VBap:
            //VBAP ----------------------------------------------------
            processVBAP(*jackCli, ins, outs, nframes, sizeInputs, sizeOutputs);
            break;
        case DBap:
            break;
        case HRTF_LOW:
        case HRTF_HIGH:
            processHRTF(*jackCli, ins, outs, nframes, sizeInputs, sizeOutputs);
            break;
        case STEREO:
            processSTEREO(*jackCli, ins, outs, nframes, sizeInputs, sizeOutputs);
            break;
        default:
            jassertfalse;
            break;
    }

    // Noise Sound-----------------------------------------------
    if (jackCli->noiseSound) {
        addNoiseSound(*jackCli, outs, nframes, sizeOutputs);
    }

    //================ OUTPUTS ==============================================
    muteSoloVuMeterGainOut(*jackCli, outs, nframes, sizeOutputs, jackCli->masterGainOut);
        
    jackCli->overload = false;
    return 0;
}


//=========================================================================================
//CALLBACK FUNCTION
//=========================================================================================
void session_callback (jack_session_event_t *event, void *arg)
{
    jackClientGris* jackCli = (jackClientGris*)arg;
    
    char retval[100];
    jack_client_log("session notification\n");
    jack_client_log("path %s, uuid %s, type: %s\n", event->session_dir, event->client_uuid, event->type == JackSessionSave ? "save" : "quit");
    
    snprintf(retval, 100, "jack_simple_session_client %s", event->client_uuid);
    event->command_line = strdup (retval);
    
    jack_session_reply(jackCli->client, event);
    
    jack_session_event_free (event);
}

int graph_order_callback ( void * arg)
{
    jackClientGris* jackCli = (jackClientGris*)arg;
    jack_client_log("graph_order_callback : ");
    jackCli->updateClientPortAvailable(true);
    jack_client_log("done \n");
    return 0;
}

int xrun_callback ( void * arg)
{
    jackClientGris* jackCli = (jackClientGris*)arg;
    jackCli->overload = true;
    jack_client_log("xrun_callback \n");
    return 0;
}

void jack_shutdown (void *arg)
{
    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "FATAL ERROR", "Please check :\n - Buffer Size\n - Sample Rate\n - Inputs/Outputs");
    jack_client_log("\n===================\nFATAL ERROR JACK\n===================\n\n");
    exit(1);
}

int sample_rate_callback(jack_nframes_t nframes, void *arg)
{
    jack_client_log("sample_rate_callback : %" PRIu32 "\n", nframes);
    return 0;
}

void client_registration_callback(const char *name, int regist, void *arg)
{
    jackClientGris* jackCli = (jackClientGris*)arg;
    jack_client_log("client_registration_callback : %s : " ,name);
    if(!strcmp(name, ClientNameIgnor)){
        jack_client_log("ignored\n");
        return;
    }
    
    jackCli->lockListClient.lock();
    if(regist){
        Client cli;
        cli.name = name;

        jackCli->listClient.push_back(cli);
        jack_client_log("saved\n");
    }else{
        for( vector<Client>::iterator iter = jackCli->listClient.begin(); iter != jackCli->listClient.end(); ++iter )
        {
            if( iter->name == String(name) )
            {
                jackCli->listClient.erase( iter );
                jack_client_log("deleted\n");
                break;
            }
        }
    }
    jackCli->lockListClient.unlock();
}

void latency_callback(jack_latency_callback_mode_t  mode, void *arg)
{
    switch (mode) {
        case JackCaptureLatency:
             jack_client_log("latency_callback : JackCaptureLatency %" PRIu32 "\n", mode);
            break;
         
        case JackPlaybackLatency:
            jack_client_log("latency_callback : JackPlaybackLatency %" PRIu32 "\n", mode);
            break;
            
        default:
            jack_client_log("latency_callback : ");
            break;
    }
}

void port_registration_callback (jack_port_id_t a, int regist, void * arg)
{
    //jackClientGris* jackCli = (jackClientGris*)arg;
    jack_client_log("port_registration_callback : %" PRIu32 " : " ,a);
    if(regist){
        jack_client_log("saved \n");
    }else{
        jack_client_log("deleted\n");
    }
}


void port_connect_callback(jack_port_id_t a, jack_port_id_t b, int connect, void* arg)
{
    jackClientGris* jackCli = (jackClientGris*)arg;
    jack_client_log("port_connect_callback : ");
    if(connect){
        //Stop Auto connection with system...
        if(!jackCli->autoConnection){
            string nameClient = jack_port_name(jack_port_by_id(jackCli->client,a));
            string tempN = jack_port_short_name(jack_port_by_id(jackCli->client,a));
            nameClient =   nameClient.substr(0,nameClient.size()-(tempN.size()+1));
            if((nameClient != ClientName && nameClient!= ClientNameSys) || nameClient== ClientNameSys){
                jack_disconnect(jackCli->client, jack_port_name(jack_port_by_id(jackCli->client,a)), jack_port_name(jack_port_by_id(jackCli->client,b)));
            }
        }
        jack_client_log("Connect ");
    }else{
        jack_client_log("Disconnect ");
    }
    jack_client_log("%" PRIu32 " <> %" PRIu32 "\n", a,b);
    return;
}


static float **
getSamplesFromWavFile(String filename) {
    int** wavData;
    float factor = powf(2.0f, 32.0f);
    AudioFormat *wavAudioFormat;
    File file = File(filename); 
    wavAudioFormat = new WavAudioFormat(); 
    FileInputStream *stream = file.createInputStream();
    AudioFormatReader *audioFormatReader = wavAudioFormat->createReaderFor(stream, true);
    wavData = new int* [3]; 
    wavData[0] = new int[audioFormatReader->lengthInSamples];
    wavData[1] = new int[audioFormatReader->lengthInSamples];
    wavData[2] = 0; 
    audioFormatReader->read(wavData, 2, 0, (int)audioFormatReader->lengthInSamples, false);
    float **samples = (float **)malloc(2 * sizeof(float *));
    for (int i = 0; i < 2; i++) {
        samples[i] = (float *)malloc(audioFormatReader->lengthInSamples * sizeof(float));
        for (int j=0; j<audioFormatReader->lengthInSamples; j++) {
            samples[i][j] = wavData[i][j] / factor;
        }
    }
    delete wavAudioFormat;
    delete stream;
    return samples;
}

//=================================================================================================================
// jackClientGris
//=================================================================================================================
jackClientGris::jackClientGris(unsigned int bufferS) {
    
    //INIT variable and clear Array========================
    this->noiseSound = false;
    this->clientReady = false;
    this->autoConnection = false;
    this->overload = false;
    this->masterGainOut = 1.0f;
    this->pinkNoiseGain = 0.1f;
    this->processBlockOn = true;
    this->modeSelected = VBap;
    this->recording = false;
    this->hrtfOn = false; // Do we need this var?

    for (unsigned int i=0; i < MaxInputs; ++i) {
        this->vbapSourcesToUpdate[i] = 0;
    }

    //---------------------------------------------------
    // Initialize impulse responses for HRTF
    //---------------------------------------------------
    int files_per_folder[14];
    this->hrtf_left = (float ***)malloc(14 * sizeof(float **));
    this->hrtf_right = (float ***)malloc(14 * sizeof(float **));
    for (int i=0; i<14; i++) {
#ifdef __linux__
        String cwd = File::getCurrentWorkingDirectory().getFullPathName();
        String folder = cwd + "/../../Resources/hrtf_compact/elev" + String((i-4)*10);
#else
        String cwd = File::getSpecialLocation(File::currentApplicationFile).getFullPathName();
        String folder = cwd + "/Contents/Resources/hrtf_compact/elev" + String((i-4)*10);
#endif
        if (File(folder).isDirectory()) {
            Array<File> result;
            int howmany = File(folder).findChildFiles(result,
                                                      File::findFiles|File::ignoreHiddenFiles,
                                                      false,
                                                      "*.wav");
            result.sort();
            files_per_folder[i] = howmany;
            this->hrtf_left[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
            this->hrtf_right[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
            for (int j=0; j<howmany; j++) {
                float **stbuf = getSamplesFromWavFile(result[j].getFullPathName());
                this->hrtf_left[i][j] = (float *)malloc(128 * sizeof(float));
                this->hrtf_right[i][j] = (float *)malloc(128 * sizeof(float));
                for (int k=0; k<128; k++) {
                    this->hrtf_left[i][j][k] = stbuf[0][k];
                    this->hrtf_right[i][j][k] = stbuf[1][k];
                }
            }
            for (int j=0; j<(howmany-1); j++) {
                this->hrtf_left[i][howmany+j] = (float *)malloc(128 * sizeof(float));
                this->hrtf_right[i][howmany+j] = (float *)malloc(128 * sizeof(float));
                for (int k=0; k<128; k++) {
                    this->hrtf_left[i][howmany+j][k] = this->hrtf_right[i][howmany-2-j][k];
                    this->hrtf_right[i][howmany+j][k] = this->hrtf_left[i][howmany-2-j][k];
                }
            }
        }
    }

    /* Compute magnitudes and unwrapped phase for each impulse response. */
    float re, im, ma, ph;
    int hsize = 64;
    int n8 = 128 >> 3;
    float outframe[128];
    for (int i=0; i<128; i++) {
        outframe[i] = 0.0;
    }
    float real[hsize], imag[hsize], magn[hsize], freq[hsize];
    for (int i=0; i<hsize; i++) {
        real[i] = imag[i] = magn[i] = freq[i] = 0.0;
    }
    this->twiddle = (float **)malloc(4 * sizeof(float *));
    for(int i=0; i<4; i++)
        this->twiddle[i] = (float *)malloc(n8 * sizeof(float));
    fft_compute_split_twiddle(this->twiddle, 128);

    this->mag_left = (float ***)realloc(this->mag_left, 14 * sizeof(float **));
    this->ang_left = (float ***)realloc(this->ang_left, 14 * sizeof(float **));
    this->mag_right = (float ***)realloc(this->mag_right, 14 * sizeof(float **));
    this->ang_right = (float ***)realloc(this->ang_right, 14 * sizeof(float **));
    for (int i=0; i<14; i++) {
        int howmany = files_per_folder[i];
        this->mag_left[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
        this->ang_left[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
        this->mag_right[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
        this->ang_right[i] = (float **)malloc((howmany*2-1) * sizeof(float *));
        for (int j=0; j<(howmany*2-1); j++) {
            this->mag_left[i][j] = (float *)malloc(hsize * sizeof(float));
            this->ang_left[i][j] = (float *)malloc(hsize * sizeof(float));
            this->mag_right[i][j] = (float *)malloc(hsize * sizeof(float));
            this->ang_right[i][j] = (float *)malloc(hsize * sizeof(float));

            /* Left channel */
            realfft_split(this->hrtf_left[i][j], outframe, 128, this->twiddle);
            real[0] = outframe[0];
            imag[0] = 0.0;
            for (int k=1; k<hsize; k++) {
                real[k] = outframe[k];
                imag[k] = outframe[128 - k];
            }
            for (int k=0; k<hsize; k++) {
                re = real[k];
                im = imag[k];
                ma = sqrtf(re*re + im*im);
                ph = atan2f(im, re);
                while (ph > M_PI) ph -= (M_PI * 2.0f);
                while (ph < -M_PI) ph += (M_PI * 2.0f);
                this->mag_left[i][j][k] = ma;
                this->ang_left[i][j][k] = ph;
            }
            /* Right channel */
            realfft_split(this->hrtf_right[i][j], outframe, 128, this->twiddle);
            real[0] = outframe[0];
            imag[0] = 0.0;
            for (int k=1; k<hsize; k++) {
                real[k] = outframe[k];
                imag[k] = outframe[128 - k];
            }
            for (int k=0; k<hsize; k++) {
                re = real[k];
                im = imag[k];
                ma = sqrtf(re*re + im*im);
                ph = atan2f(im, re);
                while (ph > M_PI) ph -= (M_PI * 2.0f);
                while (ph < -M_PI) ph += (M_PI * 2.0f);
                this->mag_right[i][j][k] = ma;
                this->ang_right[i][j][k] = ph;
            }
        }
    }

    this->setHrtfImpulseLength(128);
    //---------------------------------------------------

    //---------------------------------------------------
    // Initialize STEREO data
    //---------------------------------------------------
    for (unsigned int i=0; i < MaxInputs; ++i) {
        this->last_azi[i] = 0.0f;
    }

    //---------------------------------------------------
    // Initialize highpass filter delay samples
    //---------------------------------------------------
    for (unsigned int i=0; i<MaxOutputs; i++) {
        this->x1[i] = 0.0;
        this->x2[i] = 0.0;
        this->x3[i] = 0.0;
        this->x4[i] = 0.0;
        this->y1[i] = 0.0;
        this->y2[i] = 0.0;
        this->y3[i] = 0.0;
        this->y4[i] = 0.0;
    }

    this->listClient = vector<Client>();
    
    this->soloIn = false;
    this->soloOut = false;

    this->inputsPort = vector<jack_port_t *>();
    this->outputsPort = vector<jack_port_t *>();
    this->interMaster = 0.8f;
    this->maxOutputPatch = 0;

    //--------------------------------------------------
    //open a client connection to the JACK server. Start server if it is not running.
    //--------------------------------------------------
    jack_options_t  options = JackUseExactName;
    jack_status_t   status;
    
    jack_client_log("\n========================== \n");
    jack_client_log("Start Jack Client \n");
    jack_client_log("========================== \n");
    
    this->client = jack_client_open (ClientName, options, &status, DriverNameSys);
    if (this->client == NULL) {
        
        jack_client_log("\nTry again...\n");

        options = JackServerName;
        this->client = jack_client_open (ClientName, options, &status, DriverNameSys);
        if (this->client == NULL) {
            jack_client_log("\n\n\n======jack_client_open() failed, status = 0x%2.0x\n", status);
            if (status & JackServerFailed) {
                jack_client_log("\n\n\n======Unable to connect to JACK server\n");
            }
        }
    }
    if (status & JackServerStarted) {
        jack_client_log("\n===================\njackdmp wasn't running so it was started\n===================\n");
    }
    if (status & JackNameNotUnique) {
        ClientName = jack_get_client_name(this->client);
        jack_client_log("\n\n\n======chosen name already existed, new unique name `%s' assigned\n", ClientName);
    }
    
    //--------------------------------------------------
    //register callback, ports
    //--------------------------------------------------
    jack_on_shutdown                        (this->client, jack_shutdown, this);
    jack_set_process_callback               (this->client, process_audio, this);
    jack_set_client_registration_callback   (this->client, client_registration_callback, this);
  
    jack_set_session_callback               (this->client, session_callback, this);
    jack_set_port_connect_callback          (this->client, port_connect_callback, this);
    jack_set_port_registration_callback     (this->client, port_registration_callback, this);
    jack_set_sample_rate_callback           (this->client, sample_rate_callback, this);
    
    jack_set_graph_order_callback           (this->client, graph_order_callback, this);
    jack_set_xrun_callback                  (this->client, xrun_callback, this);
    jack_set_latency_callback               (this->client, latency_callback, this);
    
    //Default buffer size 1024
    jack_set_buffer_size(this->client, bufferS);
    
    sampleRate = jack_get_sample_rate (this->client);
    bufferSize = jack_get_buffer_size (this->client);
    
    jack_client_log("engine sample rate: %" PRIu32 "\n", sampleRate);
    jack_client_log("engine buffer size: %" PRIu32 "\n", jack_get_buffer_size(this->client));
    
    
    //--------------------------------------------------
    //Prepare pink noise
    //--------------------------------------------------
    srand(time(NULL));
    this->c0 = this->c1 = this->c2 = this->c3 = this->c4 = this->c5 = this->c6 = 0.0;
    
    //--------------------------------------------------
    //Print Inputs Ports available
    //--------------------------------------------------
    const char **ports = jack_get_ports (this->client, NULL, NULL, JackPortIsInput);
    if (ports == NULL) {
        jack_client_log("\n======NO Input PORTS\n");
        return;
    }
    this->numberInputs = 0;
    jack_client_log("Ports I ================\n\n");
    while (ports[this->numberInputs]){
        jack_client_log("%s\n", ports[this->numberInputs]);
        this->numberInputs+=1;
    }
    jack_free (ports);
    jack_client_log("\n%d =====================\n\n", this->numberInputs);
    
    
    //--------------------------------------------------
    //Print Outputs Ports available
    //--------------------------------------------------
    ports = jack_get_ports (client, NULL, NULL, JackPortIsOutput);
    if (ports == NULL) {
        jack_client_log("\n======NO Outputs PORTS\n");
        return;
    }
    this->numberOutputs = 0;
    jack_client_log("Ports O ================\n\n");
    while (ports[this->numberOutputs]){
        jack_client_log("%s\n", ports[this->numberOutputs]);
        this->numberOutputs+=1;
    }
    jack_free (ports);
    jack_client_log("\n%d =====================\n\n", this->numberOutputs);

    
    //--------------------------------------------------
    // Activate client and connect the ports. Playback ports are "input" to the backend, and capture ports are "output" from it.
    //--------------------------------------------------
    if (jack_activate (this->client)) {
        jack_client_log("\n\n\n======cannot activate client");
        return;
    }
    
    jack_client_log("\n========================== \n");
    jack_client_log("Jack Client Run \n");
    jack_client_log("========================== \n");
    
    this->clientReady = true;
}

void jackClientGris::setHrtfImpulseLength(int length) {
    this->HrtfImpulseLength = length;
    this->hrtf_sample_count = 0;
    for (unsigned int i=0; i<MaxInputs; i++) {
        this->hrtf_count[i] = 0;
        this->hrtf_last_azi[i] = 0.0f;
        this->hrtf_last_ele[i] = 0.0f;
        for (int j = 0; j<128; j++) {
            this->hrtf_input_tmp[i][j] = 0.0f;
            this->previous_impulses[i][0][j] = 0.0f;
            this->previous_impulses[i][1][j] = 0.0f;
            this->current_impulses[i][0][j] = 0.0f;
            this->current_impulses[i][1][j] = 0.0f;
        }
    }
}

void jackClientGris::prepareToRecord()
{
    int num_of_channels;
    if (this->outputsPort.size() < 1) {
        return;
    }

    this->recording = false;
    this->indexRecord = 0;

    String channelName;
    File fileS = File(this->recordPath);
    String fname = fileS.getFileNameWithoutExtension();
    String extF = fileS.getFileExtension();
    String parent = fileS.getParentDirectory().getFullPathName();

    if (this->modeSelected == VBap) {
        num_of_channels = this->outputsPort.size();
        for (int i  = 0; i < num_of_channels; ++i) {
            if (int_vector_contains(this->outputPatches, i+1)) {
                channelName = parent + "/" + fname + "_" + String(i+1).paddedLeft('0', 3) + extF;
                File fileC = File(channelName);
                this->recorder[i].startRecording(fileC, this->sampleRate, extF);
            }
        }
    } else if (this->modeSelected == HRTF_LOW || this->modeSelected == HRTF_HIGH || this->modeSelected == STEREO) {
        num_of_channels = 2;
        for (int i  = 0; i < num_of_channels; ++i) {
            channelName = parent + "/" + fname + "_" + String(i+1).paddedLeft('0', 3) + extF;
            File fileC = File(channelName);
            this->recorder[i].startRecording(fileC, this->sampleRate, extF);
        }
    }
}

void jackClientGris::addRemoveInput(int number)
{
    if(number < this->inputsPort.size()){
        while(number < this->inputsPort.size()){
            jack_port_unregister(client, this->inputsPort.back());
            this->inputsPort.pop_back();
        }
    }else{
        while(number > this->inputsPort.size()){
            String nameIn = "input";
            nameIn+= String(this->inputsPort.size() +1);
            jack_port_t* newPort = jack_port_register(this->client,  nameIn.toUTF8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
            this->inputsPort.push_back(newPort);
        }
    }
    
    connectedGristoSystem();
}

void jackClientGris::clearOutput()
{
    int outS = (int)this->outputsPort.size();
    for(int i = 0; i < outS; i++){
        jack_port_unregister(client, this->outputsPort.back());
        this->outputsPort.pop_back();
    }
}

bool jackClientGris::addOutput(int outputPatch)
{
    if (outputPatch > this->maxOutputPatch)
        this->maxOutputPatch = outputPatch;
    String nameOut = "output";
    nameOut+= String(this->outputsPort.size() +1);

    jack_port_t* newPort = jack_port_register(this->client,  nameOut.toUTF8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput,  0);
    this->outputsPort.push_back(newPort);
    connectedGristoSystem();
    return true;
}

void jackClientGris::removeOutput(int number)
{
    jack_port_unregister(client, this->outputsPort.at(number));
    this->outputsPort.erase(this->outputsPort.begin()+number);
}

void jackClientGris::connectedGristoSystem()
{
    String nameOut;
    this->clearOutput();
    for (unsigned int i = 0; i < this->maxOutputPatch; i++) {
        nameOut = "output";
        nameOut += String(this->outputsPort.size() + 1);
        jack_port_t* newPort = jack_port_register(this->client,  nameOut.toUTF8(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput,  0);
        this->outputsPort.push_back(newPort);
    }

    const char ** portsOut = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char ** portsIn = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    
    int i=0;
    int j=0;
    //DisConnect jackClientGris to system---------------------------------------------------
    while (portsOut[i]){
        if(getClientName(portsOut[i]) == ClientName)    //jackClient
        {
            j=0;
            while(portsIn[j]){
                if(getClientName(portsIn[j]) == ClientNameSys){ //system
                    jack_disconnect(this->client, portsOut[i] ,portsIn[j]);
                }
                j+=1;
            }
        }
        i+=1;
    }
    
    i=0;
    j=0;
    //Connect jackClientGris to system---------------------------------------------------
    while (portsOut[i]){
        if(getClientName(portsOut[i]) == ClientName)    //jackClient
        {
            while(portsIn[j]){
                if(getClientName(portsIn[j]) == ClientNameSys){ //system
                    jack_connect (this->client, portsOut[i] ,portsIn[j]);
                    j+=1;
                    break;
                }
                j+=1;
            }
        }
        i+=1;
    }

    // Build output patch list
    this->outputPatches.clear();
    for (unsigned int i = 0; i < this->outputsPort.size(); i++) {
        if (this->listSpeakerOut[i].outputPatch != 0) {
            this->outputPatches.push_back(this->listSpeakerOut[i].outputPatch);
        }
    }

    jack_free(portsIn);
    jack_free(portsOut);

}

bool jackClientGris::initSpeakersTripplet(vector<Speaker *>  listSpk,
                                          int dimensions, bool needToComputeVbap)
{
    int j;
    if(listSpk.size() <= 0) {
        return false;
    }

    this->processBlockOn = false;

    ls lss[MAX_LS_AMOUNT];
    int outputPatches[MAX_LS_AMOUNT];

    for(unsigned int i = 0; i < listSpk.size() ; i++) {
        for (j = 0; j < MAX_LS_AMOUNT; j++) {
            if (listSpk[i]->getOutputPatch() == listSpeakerOut[j].outputPatch && !listSpeakerOut[j].directOut) {
                break;
            }
        }
        lss[i].coords.x = listSpeakerOut[j].x;
        lss[i].coords.y = listSpeakerOut[j].y;
        lss[i].coords.z = listSpeakerOut[j].z;
        lss[i].angles.azi = listSpeakerOut[j].azimuth;
        lss[i].angles.ele = listSpeakerOut[j].zenith;
        lss[i].angles.length = listSpeakerOut[j].radius;
        outputPatches[i] = listSpeakerOut[j].outputPatch;
    }

    if (needToComputeVbap) {
        this->paramVBap = init_vbap_from_speakers(lss, listSpk.size(),
                                                  dimensions, outputPatches,
                                                  this->maxOutputPatch, NULL);
    }

    for(unsigned int i = 0; i < MaxInputs ; i++){
        listSourceIn[i].paramVBap = copy_vbap_data(this->paramVBap);
    }

    int **triplets;
    int num = vbap_get_triplets(listSourceIn[0].paramVBap, &triplets);
    vbap_triplets.clear();
    for (int i=0; i<num; i++) {
        vector <int> row;
        for (int j=0; j<3; j++) {
            row.push_back(triplets[i][j]);
        }
        vbap_triplets.push_back(row);
    }

    for (int i=0; i<num; i++) {
        free(triplets[i]);
    }
    free(triplets);

    this->connectedGristoSystem();

    this->processBlockOn = true;
    return true;
}

void jackClientGris::updateSourceVbap(int idS)
{
    if (this->vbapDimensions == 3) {
        if (listSourceIn[idS].paramVBap != nullptr) {
            vbap2_flip_y_z(listSourceIn[idS].azimuth, listSourceIn[idS].zenith,
                           listSourceIn[idS].aziSpan, listSourceIn[idS].zenSpan,
                           listSourceIn[idS].paramVBap);
        }
    } else if (this->vbapDimensions == 2) {
        if (listSourceIn[idS].paramVBap != nullptr) {
            vbap2(listSourceIn[idS].azimuth, 0.0,
                  listSourceIn[idS].aziSpan, 0.0,
                  listSourceIn[idS].paramVBap);
        }
    }
}

void jackClientGris::disconnectAllClient()
{
    const char ** portsOut = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char ** portsIn = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    
    int i=0;
    int j=0;
    
    //Disconencted ALL ------------------------------------------------
    while (portsOut[i]){
        j = 0;
        while(portsIn[j]){
            jack_disconnect(this->client, portsOut[i] ,portsIn[j]);
            j+=1;
        }
        i+=1;
    }
    this->lockListClient.lock();
    for (auto&& cli : this->listClient)
    {
        cli.connected = false;
    }
    this->lockListClient.unlock();
    jack_free(portsIn);
    jack_free(portsOut);
}

void jackClientGris::autoConnectClient()
{
    
    disconnectAllClient();
    connectedGristoSystem();
    
    const char ** portsOut = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char ** portsIn = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);
    
    int i=0;
    int j=0;
    int startJ = 0;
    int endJ = 0;
    
    //Connect other client to jackClientGris------------------------------------------
    this->autoConnection = true;
    this->lockListClient.lock();
    for (auto&& cli : this->listClient)
    {
        i=0;
        j=0;
        
        String nameClient = cli.name;
        startJ = cli.portStart-1;
        endJ = cli.portEnd;
        while (portsOut[i]){
            if(nameClient == getClientName(portsOut[i]))
            {
                while(portsIn[j]){
                    if(getClientName(portsIn[j]) == ClientName){
                        if(j>= startJ && j<endJ){
                            jack_connect (this->client, portsOut[i] ,portsIn[j]);
                            cli.connected = true;
                            j+=1;
                            break;
                        }else{
                            j+=1;
                        }
                    }else{
                        j+=1;
                        startJ+=1;
                        endJ+=1;
                    }
                }
            }
            i+=1;
        }
    }
    this->lockListClient.unlock();
    this->autoConnection = false;
    
    jack_free(portsIn);
    jack_free(portsOut);
}

void jackClientGris::connectionClient(String name, bool connect)
{
    const char ** portsOut = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    const char ** portsIn = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);

    int i=0;
    int j=0;
    int startJ = 0;
    int endJ = 0;
    bool conn = false;
    this->updateClientPortAvailable(false);
    //Disconencted Client------------------------------------------------
    while (portsOut[i]){
        if(getClientName(portsOut[i]) == name)
        {
            j = 0;
            while(portsIn[j]){
                if(getClientName(portsIn[j]) == ClientName){ //jackClient
                    jack_disconnect(this->client, portsOut[i] ,portsIn[j]);
                }
                j+=1;
            }
        }
        i+=1;
    }

    for (auto&& cli : this->listClient)
    {
        if(cli.name == name){
            cli.connected = false;
        }
    }

    
    connectedGristoSystem();
    if(!connect){ return ; }
    //Connect other client to jackClientGris------------------------------------------
    this->autoConnection = true;

    for (auto&& cli : this->listClient)
    {
        i=0;
        j=0;
        String nameClient = cli.name;
        startJ = cli.portStart-1;
        endJ = cli.portEnd;

        while (portsOut[i]){
            if(nameClient == name && nameClient == getClientName(portsOut[i]))
            {
                while(portsIn[j]){
                    if(getClientName(portsIn[j]) == ClientName){
                        if(j>= startJ && j<endJ){
                            jack_connect (this->client, portsOut[i] ,portsIn[j]);
                            conn = true;
                            j+=1;
                            break;
                        }else{
                            j+=1;
                        }
                    }else{
                        j+=1;
                        startJ+=1;
                        endJ+=1;
                    }
                }
                cli.connected = conn;
            }
            i+=1;
        }
    }

    this->autoConnection = false;
    
    jack_free(portsIn);
    jack_free(portsOut);
}


string jackClientGris::getClientName(const char * port)
{
    if(port){
        jack_port_t *  tt = jack_port_by_name(this->client, port);
        if(tt){
            string nameClient = jack_port_name(tt);
            string tempN = jack_port_short_name(tt);
            return nameClient.substr(0,nameClient.size()-(tempN.size()+1));
        }
    }return "";
}

void jackClientGris::updateClientPortAvailable(bool fromJack)
{
    const char ** portsOut = jack_get_ports (this->client, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);
    int i = 0;

    for (auto&& cli : this->listClient)
    {
        cli.portAvailable = 0;
    }
    
    while (portsOut[i]){
        string nameCli = getClientName(portsOut[i]);
        if(nameCli != ClientName &&  nameCli != ClientNameSys){
            for (auto&& cli : this->listClient)
            {
                if(cli.name == nameCli){
                    cli.portAvailable+=1;
                }
            }
        }
        i++;
    }

    unsigned int start = 1;
    for (auto&& cli : this->listClient) {
        if (!fromJack) {
            cli.initialized = true;
        }
        if (cli.portStart == 0 || cli.portEnd == 0 || !cli.initialized) { // ports not initialized.
            cli.portStart = start;
            cli.portEnd = start + cli.portAvailable - 1;
            start += cli.portAvailable;
        } else if ((cli.portStart >= cli.portEnd) || (cli.portEnd - cli.portStart > cli.portAvailable)) { // portStart bigger than portEnd.
            cli.portStart = start;
            cli.portEnd = start + cli.portAvailable - 1;
            start += cli.portAvailable;
        } else {
            if (this->listClient.size() > 1) {
                int pos = 0;
                bool somethingBad = false;
                for (unsigned int c=0; c<this->listClient.size(); c++) {
                    if (this->listClient[c].name == cli.name) {
                        pos = c;
                        break;
                    }
                }
                if (pos == 0) {
                    somethingBad = false;
                } else if (pos >= this->listClient.size()) {
                    somethingBad = true; // Never supposed to get here.
                } else {
                    for (int k=0; k<pos; k++) {
                        struct Client clicmp = this->listClient[k];
                        if (clicmp.name != cli.name && cli.portStart > clicmp.portStart && cli.portStart < clicmp.portEnd) {
                            somethingBad = true;
                        } else if (clicmp.name != cli.name && cli.portEnd > clicmp.portStart && cli.portEnd < clicmp.portEnd) {
                            somethingBad = true;
                        }
                    }

                }

                if (somethingBad) {  // ports overlap other client ports.
                    cli.portStart = start;
                    cli.portEnd = start + cli.portAvailable - 1;
                    start += cli.portAvailable;
                } else {
                    // If everything goes right, we keep portStart and portEnd for this client.
                    start += cli.portEnd;
                }
            }
        }
        if (cli.portStart > this->inputsPort.size()) {
            //cout << "Not enough inputs, client can't connect!" << " " << cli.portStart << " " << this->inputsPort.size() << endl;
        }
    }

    jack_free(portsOut);
}

unsigned int jackClientGris::getPortStartClient(String nameClient)
{
    for (auto&& it : this->listClient)
    {
        if(it.name==nameClient){
            return it.portStart;
        }
    }
    return 0;
}

jackClientGris::~jackClientGris() {
    jack_deactivate(this->client);
    for(unsigned int i = 0 ; i < this->inputsPort.size() ; i++){
        jack_port_unregister(this->client, this->inputsPort[i]);
    }
    
    for(unsigned int i = 0 ; i < this->outputsPort.size()  ; i++){
        jack_port_unregister(this->client, this->outputsPort[i]);
    }
    jack_client_close(this->client);

    // Free memory used by the HRTF algorithm.
    for (int i=0; i<14; i++) {
        int howmany = this->hrtf_how_many_files_per_folder[i] * 2 - 1;
        for (int j=0; j<howmany; j++) {
            free(this->hrtf_left[i][j]);
            free(this->hrtf_right[i][j]);
            free(this->mag_left[i][j]);
            free(this->ang_left[i][j]);
            free(this->mag_right[i][j]);
            free(this->ang_right[i][j]);
        }
        free(this->hrtf_left[i]);
        free(this->hrtf_right[i]);
        free(this->mag_left[i]);
        free(this->ang_left[i]);
        free(this->mag_right[i]);
        free(this->ang_right[i]);
    }
    free(this->hrtf_left);
    free(this->hrtf_right);
    free(this->mag_left);
    free(this->ang_left);
    free(this->mag_right);
    free(this->ang_right);
}
