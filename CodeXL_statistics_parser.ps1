$statistic = Get-Content c:\Users\Павел\AppData\Local\Temp\AMD\CodeXL\Project1_KernelOutput\bvh_aio_cl\IntersectClosest\Statistics.cxltxt
$csv_table = ConvertFrom-Csv $statistic -Delimiter ';'
$csv_table