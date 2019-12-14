#include "sensor.h"

const char *list_name[] = {"Valid",
                           "Out of Range",
                           "Stuck"};

/**
 * Head pointers to three linked lists used for storing three types of sensors
 * retrieved from the specified input file.
 */
Node_t *sensor_list_head_array[MAX_SENSOR_LISTS] = {NULL};

void run_main_sensor_algorithm(char *in_file_name,
                               char *out_file_name,
                               float high_range,
                               Boolean use_high_range,
                               float low_range,
                               Boolean use_low_range,
                               float stuck_range,
                               Boolean use_stuck)
{
    Boolean end_of_file_reached = FALSE;
    float fused_sensor_value;
    float sensor_value;
    char sensor_name[MAX_SENSOR_NAME_SIZE];
    int time_in_minutes = 0;
    int current_time = -1;
    int lines_read = 1;

    do
    {
        end_of_file_reached = parser_parse_csv_file(in_file_name,
                                                    lines_read,
                                                    &time_in_minutes,
                                                    &sensor_name[0],
                                                    &sensor_value);
        if (current_time == -1)
        {
            /**
             * This is the first time that we are looking at, we want to get
             * other sensors that have updated at the same time as well.
             */
            current_time = time_in_minutes;
        }
        else
        {
            /**
             * We want to check to see that the value we have just read was
             * updated at the same time as all the other values we have
             * collected.
             */
            if (time_in_minutes != current_time)
            {
                /**
                 * We have read a line that has a different time stamp than
                 * the rest (or we have hit the end of the file)
                 */
                if (use_stuck)
                {
                    determine_if_sensors_are_stuck(current_time,
                                                   stuck_range);
                }

                fused_sensor_value = do_sensor_fusion_algorithm();
                write_output_file(out_file_name,
                                  use_high_range,
                                  high_range,
                                  use_low_range,
                                  low_range,
                                  use_stuck,
                                  stuck_range,
                                  current_time,
                                  fused_sensor_value);

                current_time = -1;
            }
        }

        ++lines_read;
        update_sensor_lists(time_in_minutes,
                            sensor_name,
                            sensor_value,
                            use_high_range,
                            high_range,
                            use_low_range,
                            low_range);
    } while (!end_of_file_reached);

    /**
     * Hitting the end of the file is not the same as finding all the entries
     * with the same timestamp, so we will run the fusion algorithm after
     * hitting the end of the file since there will be no more entries.
     */
    if (use_stuck)
    {
        determine_if_sensors_are_stuck(time_in_minutes, stuck_range);
    }

    fused_sensor_value = do_sensor_fusion_algorithm();
    write_output_file(out_file_name,
                      use_high_range,
                      high_range,
                      use_low_range,
                      low_range,
                      use_stuck,
                      stuck_range,
                      time_in_minutes,
                      fused_sensor_value);
}

void update_sensor_lists(int time_in_minutes,
                         char *sensor_name,
                         float sensor_value,
                         int use_high_range,
                         int high_range,
                         int use_low_range,
                         int low_range)
{
    Node_t *node = NULL;
    int list_index;

    /**
     * Check to see if the node already exists. If it does, we will remove
     * it from the chain it belongs to, it will be re-added in a bit.
     */
    node = search_all_chains(sensor_name, &list_index);
    if (node)
    {
        sensor_list_head_array[list_index] = remove_node(sensor_list_head_array[list_index],
                                                         node);
    }

    if ((use_high_range) && (sensor_value > high_range))
    {
        /**
         * Sensor is above the high range, so we dump it into the out of
         * range list.
         */
        sensor_list_head_array[OOR_SENSOR_LIST] = append(sensor_list_head_array[OOR_SENSOR_LIST],
                                                         time_in_minutes,
                                                         sensor_name,
                                                         sensor_value);
    }
    else if ((use_low_range) && (sensor_value < low_range))
    {
        /**
         * Sensor is below the low range, so we dump it into the out of
         * range list.
         */
        sensor_list_head_array[OOR_SENSOR_LIST] = append(sensor_list_head_array[OOR_SENSOR_LIST],
                                                         time_in_minutes,
                                                         sensor_name,
                                                         sensor_value);
    }
    else
    {
        /* Sensor is fine, it gets to go into the valid list. */
        sensor_list_head_array[VALID_SENSOR_LIST] = append(sensor_list_head_array[VALID_SENSOR_LIST],
                                                           time_in_minutes,
                                                           sensor_name,
                                                           sensor_value);
    }
}

Node_t *search_all_chains(char *str, int *list_index)
{
    Node_t *node = NULL;
    int i;

    for (i = 0; i < MAX_SENSOR_LISTS; i++)
    {
        node = search_sensor_name(sensor_list_head_array[i], str);
        if (node)
        {
            (*list_index) = i;
            return node;
        }
    }

    return NULL;
}

void determine_if_sensors_are_stuck(int current_time, int stuck_value)
{
    Boolean rc = 0;
    Node_t *node = NULL;
    Node_t *next = NULL;
    int i;

    for (i = 0; i < MAX_SENSOR_LISTS; i++)
    {
        if (i == STUCK_SENSOR_LIST)
        {
            /**
             * We don't need to check the stuck sensor list. They're already
             * stuck.
             */
            continue;
        }

        node = sensor_list_head_array[i];
        while (node)
        {
            /**
             * We're moving around nodes here, so we want to make sure we're
             * pointing to the correct next node, before we do any sort of
             * manipulation.
             */
            next = node->next;
            if ((node->time_in_minutes + stuck_value) < current_time)
            {
                /**
                 * Sensor has not been updated recently enough and must be
                 * considered stuck. It will be moved to the stuck list.
                 */
                rc = move_node(node,
                               &sensor_list_head_array[i],
                               &sensor_list_head_array[STUCK_SENSOR_LIST]);
                if (!rc)
                {
                    printf("Error: Could not move sensor node \"%s\" from %s "
                           "list to %s list\n",
                           node->sensor_name,
                           list_name[i],
                           list_name[STUCK_SENSOR_LIST]);
                }
            }

            node = next;
        }
    }
}

void dump_current_lists(void)
{
    Node_t *node = NULL;
    int i;

    for (i = 0; i < MAX_SENSOR_LISTS; i++)
    {
        node = sensor_list_head_array[i];
        while (node)
        {
            switch (i)
            {
            case VALID_SENSOR_LIST:
                printf("Valid - ");
                break;

            case OOR_SENSOR_LIST:
                printf("  OOR - ");
                break;

            case STUCK_SENSOR_LIST:
                printf("Stuck - ");
                break;

            default:
                printf("Invalid list\n");
                return;
                break;
            }

            printf("Time: %d, Value: %f, Name: %s\n",
                   node->time_in_minutes,
                   node->sensor_value,
                   node->sensor_name);
            node = node->next;
        }
    }
}

void write_output_file(char *file_name,
                       int use_high_range,
                       float high_range,
                       int use_low_range,
                       float low_range,
                       int use_stuck,
                       int stuck_range,
                       int current_time,
                       float fused_sensor_value)
{
    Node_t *node;
    FILE *fp;
    int minutes;
    int hours;
    int i = 0;

    fp = fopen(file_name, OUTPUT_MODE);
    if (fp == NULL)
    {
        /**
         * If we fail to open the file, then we need to abort the program,
         * there is nothing to be done.
         */
        printf("Can't open output file %s\n", file_name);
        printf("Error - %s\n", strerror(errno));
        return;
    }

    hours = current_time / 60;
    minutes = current_time % 60;

    fprintf(fp, "\n\n");
    fprintf(fp, "--------------------------------------------------------------"
                "------------------\n");
    fprintf(fp, "Fused Sensor Algorithm run for sensors reporting at:"
                " %02d%02dH\n\n",
            hours,
            minutes);
    fprintf(fp, "Sensor Parameters\n");
    fprintf(fp, "High Limit --------- ");
    use_high_range ? fprintf(fp, "%0.4f\n", high_range) : fprintf(fp, "N/A\n");

    fprintf(fp, "Low Limit ---------- ");
    use_low_range ? fprintf(fp, "%0.4f\n", low_range) : fprintf(fp, "N/A\n");

    fprintf(fp, "Stuck Interval ----- ");
    use_stuck ? fprintf(fp, "%02d\n", stuck_range) : fprintf(fp, "N/A\n");

    fprintf(fp, "Fused Sensor Value - %0.4f\n", fused_sensor_value);

    fprintf(fp, "\n");
    fprintf(fp, "Sensor Statistics:\n");
    fprintf(fp, "--------+--------+------------+------\n");
    fprintf(fp, " Update | Status |   Value    | Name \n");
    fprintf(fp, "--------+--------+------------+------\n");
    for (i = 0; i < MAX_SENSOR_LISTS; i++)
    {

        node = sensor_list_head_array[i];
        while (node)
        {

            hours = node->time_in_minutes / 60;
            minutes = node->time_in_minutes % 60;
            fprintf(fp, " %02d%02dH  |", hours, minutes);

            switch (i)
            {
            case VALID_SENSOR_LIST:
                fprintf(fp, " Valid  |");
                break;

            case OOR_SENSOR_LIST:
                fprintf(fp, " OOR    |");
                break;

            case STUCK_SENSOR_LIST:
                fprintf(fp, " Stuck  |");
                break;

            default:
                fprintf(fp, " ERROR |");
                break;
            }

            fprintf(fp,
                    " %10.04f | %s\n",
                    node->sensor_value,
                    node->sensor_name);
            node = node->next;
        }
    }
    fprintf(fp, "--------+--------+------------+------\n");

    fclose(fp);
    return;
}

double do_sensor_fusion_algorithm(void)
{
    Node_t *node = sensor_list_head_array[VALID_SENSOR_LIST];
    /**
     * Call step one of the Fused Sensor Calculation here with a
     * pointer to the head of the valid_sensor_list
     *
     * c = count(sensor_list_head_array[VALID_SENSOR_LIST]);
     * calculate_support_degree_matrix(c,
     *    sensor_list_head_array[VALID_SENSOR_LIST]);
     */
    int no_of_sensors = count(sensor_list_head_array[VALID_SENSOR_LIST]);
    double *sensor_array = (double *)malloc(no_of_sensors * sizeof(double));
    printf("nos %d\n", no_of_sensors);
    double *sd_matrix = calculate_support_degree_matrix(node, no_of_sensors, sensor_array);
  
    struct eigen_systems *eigen = calculate_eigensystem(sd_matrix, no_of_sensors);
    
    double *contribution_rate = calculate_contribution_rate(eigen->eigen_value, no_of_sensors);
    
    int contribution_rates_to_use = determine_contribution_rates_to_use(contribution_rate, 0.5, no_of_sensors);
    
    double **principal_components_matrix = calculate_principal_components(sd_matrix, no_of_sensors, eigen->eigen_vector, contribution_rates_to_use);
    
    double *integrated_support_matrix =
        calculate_integrated_support_degree_matrix(
        principal_components_matrix,
        contribution_rate,
        contribution_rates_to_use, no_of_sensors);
    
    int result = eliminate_incorrect_data(integrated_support_matrix,
        0.5, no_of_sensors);
    
    double *weight_coefficient = calculate_weight_coefficient(integrated_support_matrix,
        no_of_sensors);

    double sensed_value;
    calculate_fused_output(weight_coefficient, sensor_array, no_of_sensors, &sensed_value);
    printf("sensed_value:%f\n", sensed_value);
    /* Return the fused sensor value. */

    free(sd_matrix);
    free(eigen->eigen_value);
    for(int i=0; i< no_of_sensors; i++){
        free(eigen->eigen_vector[i]);
    }
    free(eigen);
    free(contribution_rate);
    for(int i=0; i< no_of_sensors; i++){
        free(principal_components_matrix[i]);
    }
    free(principal_components_matrix);
    free(sensor_array);
    free(weight_coefficient);
    free(integrated_support_matrix);
    printf("============== END FUSED SENSOR ================\n");
    return sensed_value;
}
