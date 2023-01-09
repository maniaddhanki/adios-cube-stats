#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <adios2.h>
#include <mpi.h>

#include "helper.hpp"


// This will work only on 4d images with dimension of polarisation axis 1
int main(int argc, char *argv[])
{
    int naxis;
    int naxes[4];

    MPI_Init(&argc, &argv);

    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    adios2::fstream inStream("casa.bp", adios2::fstream::in_random_access,
                             MPI_COMM_WORLD);

    if (rank == 0)
    {
        getImageDimensions(inStream, naxis, naxes);
        // using MPI_Bcast, no way to define tags
        MPI_Bcast(&naxis, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(naxes, 4, MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Bcast(&naxis, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(naxes, 4, MPI_INT, 0, MPI_COMM_WORLD);
    }
    size_t spat_size = naxes[0] * naxes[1];

    if (rank == 0)
    {
        printf("#%7s %15s %10s %10s %10s %10s %10s %10s %10s\n", "Channel",
               "Frequency", "Mean", "Std", "Median", "MADFM", "1%ile", "Min",
               "Max");
        printf("#%7s %15s %10s %10s %10s %10s %10s %10s %10s\n", " ", "MHz",
               "mJy/beam", "mJy/beam", "mJy/beam", "mJy/beam", "mJy/beam",
               "mJy/beam", "mJy/beam");
    }

    int quot = naxes[2] / size;
    int channelsToRead = quot;
    int startOffset = 0;
    int rem = naxes[2] % size;
    if (rank >= size - rem)
    {
        channelsToRead = channelsToRead + 1;
        startOffset = rem - (size - rank);
    }
    int startChannel = rank * quot + startOffset;

    const adios2::Dims start = {static_cast<std::size_t>(0), static_cast<std::size_t>(startChannel), static_cast<std::size_t>(0), static_cast<std::size_t>(0)};

    const adios2::Dims count = {static_cast<std::size_t>(1), static_cast<std::size_t>(channelsToRead), static_cast<std::size_t>(naxes[0]), static_cast<std::size_t>(naxes[1])};

    // std::cout << "Rank: " << rank << "startChannel: " << startChannel << "channelsToRead: " << channelsToRead << std::endl;

    const std::vector<float> data = inStream.read<float>("data", start, count);

    /* process image one channel at a time; increment channel # in each loop */
    for (int channel = 0; channel < channelsToRead; channel++)
    {

        float sum = 0., meanval = 0., minval = 1.E33, maxval = -1.E33;
        float valid_pix = 0;

        for (size_t ii = channel * spat_size; ii < (channel + 1) * spat_size; ii++)
        {
            float val = data[ii];
            valid_pix += isnan(val) ? 0 : 1;
            val = isnan(val) ? 0.0 : val;

            sum += val; /* accumlate sum */
            if (val < minval)
                minval = val; /* find min and  */
            if (val > maxval)
                maxval = val; /* max values    */
        }
        meanval = sum / valid_pix;

        meanval *= 1000.0;
        minval *= 1000.0;
        maxval *= 1000.0;

        printf("%8d %15.6f %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f\n",
               startChannel + channel + 1, 1.0f, meanval, 0.0f, 0.0f, 0.0f, 0.0f, minval, maxval);
    }

    inStream.close();

    MPI_Finalize();
    return 0;
}
